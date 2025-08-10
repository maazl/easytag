/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2025  Marcel Mueller <github@maazl.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "acoustid.h"
#ifdef ENABLE_ACOUSTID

#include "misc.h"
#include "setting.h"
#include "file.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#include <chromaprint.h>

#include <atomic>
#include <thread>
#include <chrono>
#include <cassert>
#include <algorithm>

using namespace std;

namespace {

constexpr const int SampleRate = 11025; // Always used by ChromaPrint
constexpr const int MaxSamples = 120 * SampleRate; // Restrict to first 2 minutes.

constexpr const char* ApiKey = "1zVMCQEHzk";


static JsonObject* json_object_get_object(JsonObject* obj, const char* member)
{
	JsonNode* node = json_object_get_member(obj, member);
	if (!node || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return nullptr;
	return json_node_get_object(node);
}

static const char* json_object_get_string(JsonObject* obj, const char* member)
{
	JsonNode* node = json_object_get_member(obj, member);
	if (!node || json_node_get_node_type(node) != JSON_NODE_VALUE)
		return nullptr;
	return json_node_get_string(node);
}

static double json_object_get_double(JsonObject* obj, const char* member)
{
	JsonNode* node = json_object_get_member(obj, member);
	if (!node || json_node_get_node_type(node) != JSON_NODE_VALUE)
		return 0.;
	return json_node_get_double(node);
}

static double json_object_get_int(JsonObject* obj, const char* member)
{
	JsonNode* node = json_object_get_member(obj, member);
	if (!node || json_node_get_node_type(node) != JSON_NODE_VALUE)
		return 0;
	return json_node_get_int(node);
}

static JsonArray* json_object_get_array(JsonObject* obj, const char* member)
{
	JsonNode* node = json_object_get_member(obj, member);
	if (!node || json_node_get_node_type(node) != JSON_NODE_ARRAY)
		return nullptr;
	return json_node_get_array(node);
}

static JsonObject* json_array_get_object(JsonArray* arr, guint index)
{
	JsonNode* node = json_array_get_element(arr, index);
	if (!node || json_node_get_node_type(node) != JSON_NODE_OBJECT)
		return nullptr;
	return json_node_get_object(node);
}

static JsonObject* json_object_get_first_object(JsonObject* obj, const char* member)
{
	JsonArray* array = json_object_get_array(obj, member);
	if (!array || !json_array_get_length(array))
		return nullptr;
	return json_array_get_object(array, 0);
}

class AcoustIDImpl : public AcoustID
{
	static atomic<clock_t> LastRequest;

	ChromaprintContext* Ctx;
	gObject<SoupSession> Session;

	double Duration;
	char* Fingerprint;
	JsonNode* JsonResponse;
	gObject<GCancellable> Cancel;

	static void LimitLookupRate();
	static xStringD0 ParseArtists(JsonObject* obj);

	string CalcFingerprint(const char* filename);
	string CallLookup();
	string ParseJson(Matches& matches);

public:
	AcoustIDImpl();
	~AcoustIDImpl();
	virtual void AnalyzeFile(ET_File& file, GCancellable* cancel);
};

atomic<clock_t> AcoustIDImpl::LastRequest(0);

AcoustIDImpl::AcoustIDImpl()
:	Ctx(chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT))
,	Session(soup_session_new_with_options(SOUP_SESSION_USER_AGENT, PACKAGE_NAME " " PACKAGE_VERSION, NULL))
,	Duration(0)
, Fingerprint(nullptr)
,	JsonResponse(nullptr)
{}

AcoustIDImpl::~AcoustIDImpl()
{	if (JsonResponse)
		json_node_free(JsonResponse);
	chromaprint_dealloc(Fingerprint);
	chromaprint_free(Ctx);
}

string AcoustIDImpl::CalcFingerprint(const char* filename)
{
	char err[AV_ERROR_MAX_STRING_SIZE];
	auto avstrerr = [&err](int e) { return (const char*)av_make_error_string(&err[0], sizeof(err), e); };

	// get format from audio file
	AVFormatContext* pformat = nullptr;
	int rc = avformat_open_input(&pformat, filename, nullptr, nullptr);
	if (rc)
		return string(_("Could not open file: ")) + avstrerr(rc);
	auto format = make_unique(pformat, [](AVFormatContext* ptr) { avformat_close_input(&ptr); });
	rc = avformat_find_stream_info(format.get(), nullptr);
	if (rc < 0)
		return string(_("Could not retrieve stream info from file: ")) + avstrerr(rc);

	// Find the first audio stream
	const AVStream* stream;
	for (unsigned i = 0; i < format.get()->nb_streams; i++)
		if (format.get()->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{	stream = format->streams[i];
			goto found_stream;
		}
	return _("Could not retrieve audio stream from file.");
found_stream:

	// find & open codec
	const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
	auto codec = make_unique(avcodec_alloc_context3(decoder), [](AVCodecContext* ptr) { avcodec_free_context(&ptr); });
	rc = avcodec_parameters_to_context(codec.get(), stream->codecpar);
	if (rc != 0)
		return string("Failed to set codec parameters: ") + avstrerr(rc);
	codec->request_sample_fmt = av_get_planar_sample_fmt(codec->sample_fmt);
	rc = avcodec_open2(codec.get(), decoder, nullptr);
	if (rc)
		return strprintf("Failed to open codec for stream #%u: %s", stream->index, avstrerr(rc));

	// some codecs do not set channel layout
	if (codec->channel_layout == 0)
		codec->channel_layout = av_get_default_channel_layout(codec->channels);

	// prepare resampler
	auto swr = make_unique(swr_alloc_set_opts(NULL, AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16, SampleRate,
			codec->channel_layout, codec->sample_fmt, codec->sample_rate, 0, NULL),
		[](SwrContext* ptr) { swr_free(&ptr); });
	rc = swr_init(swr.get());
	if (rc < 0)
		return string(_("Resampler has not been properly initialized: ")) + avstrerr(rc);

	if (!chromaprint_start(Ctx, SampleRate, 1))
		return "Failed to initialize chromaprint";

	// prepare to read data
	auto frame = make_unique(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });

	uint8_t* buffer;
	int buffersize = 0;
	auto buffer_ptr = make_unique((uint8_t*)nullptr, [](uint8_t* ptr) { av_freep(&ptr); });

	// iterate through frames
	AVPacket packet{0};
	bool done = false;
	size_t total_samples = 0;
	int count;
	do
	{	rc = av_read_frame(format.get(), &packet);
		auto managed_packet = make_unique(&packet, av_packet_unref);
		if (rc == 0)
		{	if (packet.stream_index != stream->index)
				continue; // skip non audio
			rc = avcodec_send_packet(codec.get(), &packet);
		} else if (rc == AVERROR_EOF)
		{	// flush decoder
			rc = avcodec_send_packet(codec.get(), nullptr);
			done = true;
		} else
			return string(_("Error while reading audio frame: ")) + avstrerr(rc);
		if (rc)
		{	if (rc == AVERROR(EAGAIN) || rc == AVERROR_INVALIDDATA) // continue on stream error
				continue;
			else
				return string("Error sending packet to codec: ") + avstrerr(rc);
		}

		if (Cancel && g_cancellable_is_cancelled(Cancel.get()))
			return "$Aborted";

		while (avcodec_receive_frame(codec.get(), frame.get()) == 0)
		{
			// resample
			int outsamples = swr_get_delay(swr.get(), SampleRate)
				+ av_rescale_rnd(frame->nb_samples, SampleRate, frame->sample_rate, AV_ROUND_UP);

			if (outsamples > buffersize)
			{	buffer_ptr.reset();
				rc = av_samples_alloc(&buffer, NULL, 1, outsamples, AV_SAMPLE_FMT_S16, 0);
				if (rc < 0)
					return string("Memory allocation error: ") + avstrerr(rc);
				buffer_ptr.reset(buffer);
				buffersize = outsamples;
			}

			count = swr_convert(swr.get(), &buffer, buffersize, (const uint8_t**)frame->extended_data, frame->nb_samples);
			if (count < 0)
				return string("Sample rate conversion failed: ") + avstrerr(rc);

			// and process
			total_samples += count;
			if (total_samples > MaxSamples)
			{	count -= total_samples - MaxSamples;
				done = true;
			}
			if (!chromaprint_feed(Ctx, (int16_t*)buffer, count))
				return "Failed to feed chromaprint";
		}

	} while (!done);

	// flush resampler
	if (total_samples < MaxSamples)
	{	count = swr_convert(swr.get(), &buffer, buffersize, nullptr, 0);
		if (count > 0)
		{	total_samples += count;
			if (total_samples > MaxSamples)
				count -= total_samples - MaxSamples;
			if (!chromaprint_feed(Ctx, (int16_t*)buffer, count))
				return "Failed to feed chromaprint";
		}
	}

	chromaprint_get_fingerprint(Ctx, &Fingerprint);
	if (!Fingerprint)
		return _("No fingerprint for this file available.");

	return string();
}

void AcoustIDImpl::LimitLookupRate()
{	// rate limit: once per 3 seconds + 100ms for network latencies
	const constexpr clock_t clocks_per_msec = CLOCKS_PER_SEC / 1000;
	clock_t next = clock();
	clock_t current = LastRequest;
	do
	{	if (current + 3100 * clocks_per_msec > next)
			next = current + 3100 * clocks_per_msec;
	} while (!LastRequest.compare_exchange_weak(current, next));

	current = clock();
	if (current < next)
		this_thread::sleep_for((next - current) / clocks_per_msec * 1ms);
}

string AcoustIDImpl::CallLookup()
{
	if (JsonResponse)
		json_node_free(JsonResponse);
	JsonResponse = nullptr;

	gString uri(g_settings_get_string(MainSettings, "acoustid-uri"));

	/* AcoustId service seems to be broken with HTTP POST
	 * => use GET with ugly long URI instead.
	gObject<SoupMessage> message(soup_message_new(SOUP_METHOD_POST, uri));
	if (!message)
		return string(_("Invalid AcoustID request URI: ")) + uri.get();

	gString request(g_strdup_printf("{ "
		"\"client\":\"%s\", "
		"\"duration\":%u, "
		"\"fingerprint\":\"%s\", "
		"\"meta\":\"recordings tracks releases compress\" }\r\n",
		ApiKey, (unsigned)Duration, Fingerprint));

	chromaprint_dealloc(Fingerprint);
	Fingerprint = nullptr;

	soup_message_set_request(message.get(), "application/json", SOUP_MEMORY_TAKE, request, strlen(request));
	request.release(); // ownership transferred to message */

	string requri = strprintf("%s?client=%s&duration=%u&meta=recordings+tracks+releases+compress&fingerprint=%s",
		uri.get(), ApiKey, (unsigned)Duration, Fingerprint);
	//fprintf(stderr, "%s\n", requri.c_str());

	chromaprint_dealloc(Fingerprint);
	Fingerprint = nullptr;

	gObject<SoupMessage> message(soup_message_new(SOUP_METHOD_GET, requri.c_str()));
	if (!message)
		return string(_("Invalid AcoustID request URI: ")) + uri.get();

	requri.clear();
	uri.reset();

	LimitLookupRate();

	GError* error = NULL;
	gObject<GInputStream> response(soup_session_send(Session.get(), message.get(), Cancel.get(), &error));
	if (error && error->code == G_IO_ERROR_CANCELLED)
		return "$Aborted";
	if (!response)
	{	string msg = string(_("Failed to query AcoustID service: ")) + error->message;
		g_error_free(error);
		return msg;
	}
	guint status;
	g_object_get(message.get(), "status_code", &status, NULL); // soup_message_get_status is missing
	if (status / 100 != 2)
		return "Received invalid HTTP status " + to_string(status);

	gObject<JsonParser> parser(json_parser_new());
	if (!json_parser_load_from_stream(parser.get(), response.get(), NULL, &error))
	{	string msg = string(_("Failed to parse response from AcoustID service: ")) + error->message;
		g_error_free(error);
		return msg;
	}

	JsonResponse = json_parser_steal_root(parser.get());

	return string();
}

xStringD0 AcoustIDImpl::ParseArtists(JsonObject* obj)
{
	xStringD0 result;
	JsonArray* arr = json_object_get_array(obj, "artists");
	if (!arr)
		return result;

	JsonObject* artist;
	unsigned n = json_array_get_length(arr);
	switch (n)
	{case 1:
		artist = json_array_get_object(arr, 0);
		if (artist)
			result.assignNFC(json_object_get_string(artist, "name"));
	 case 0:
		return result;
	}
	// multiple artists => join
	string results;
	for (unsigned i = 0; i < n; ++i)
	{	artist = json_array_get_object(arr, i);
		if (!artist)
			continue;
		const char* name = json_object_get_string(artist, "name");
		if (!name)
			continue;
		results += name;
		if (i != n - 1)
		{	name = json_object_get_string(artist, "joinphrase");
			if (!name)
				name = " & ";
			results += name;
		}
	}
	result.assignNFC(results.c_str());
	return result;
}

string AcoustIDImpl::ParseJson(Matches& matches)
{
	JsonObject* root = json_node_get_object(JsonResponse);
	// check status
	{	const char* status = json_object_get_string(root, "status");
		if (g_strcmp0(status, "ok"))
		{	if (!status)
				return _("AcoustID service returned unexpected result.");
			JsonObject* error = json_object_get_object(root, "error");
			if (error)
			{	const char* message = json_object_get_string(error, "message");
				if (message)
					status = message;
			}
			return string(_("AcoustID service returned an error: ")) + status;
		}
	}

	unsigned recording_count = 0;
	Recording* recordings = nullptr;
	unsigned result_count;
	unsigned r = 0;

	JsonArray* results = json_object_get_array(root, "results");
	if (!results)
		goto done; // empty response is not an error

	result_count = json_array_get_length(results);
	// count recordings recursively
	for (unsigned i = 0; i < result_count; ++i)
	{
		JsonObject* result = json_array_get_object(results, i);
		if (!result)
			continue;
		JsonArray* recs = json_object_get_array(result, "recordings");
		if (!recs)
			continue;
		recording_count += json_array_get_length(recs);
	}

	recordings = new Recording[recording_count];

	for (unsigned i = 0; i < result_count; ++i)
	{
		JsonObject* result = json_array_get_object(results, i);
		if (!result)
			continue;
		JsonArray* recs = json_object_get_array(result, "recordings");
		if (!recs)
			continue;
		unsigned rec_count = json_array_get_length(recs);

		for (unsigned j = 0; j < rec_count; ++j)
		{
			Recording& rec = recordings[r++];
			JsonObject* obj = json_array_get_object(recs, j);
			if (!obj)
				continue;

			rec.score = json_object_get_double(result, "score");

			rec.id = Guid::parse(json_object_get_string(obj, "id"));
			rec.artist = ParseArtists(obj);
			rec.title.assignNFC(json_object_get_string(obj, "title"));
			rec.duration = json_object_get_double(obj, "duration");

			// reduce score for length mismatch
			{	double degrade = rec.duration - Duration;
				degrade *= degrade;
				degrade /= degrade + 250; // 5s difference degrade by 10%
				if (degrade > 0 && degrade <= 1)
					rec.score *= 1 - degrade;
			}

			rec.release_count = 0;
			JsonArray* releases = json_object_get_array(obj, "releases");
			if (!releases)
				continue;

			rec.release_count = json_array_get_length(releases);
			Release* rels = new Release[rec.release_count];
			rec.releases.reset(rels);

			for (unsigned k = 0; k < rec.release_count; ++k)
			{
				obj = json_array_get_object(releases, k);
				if (!obj)
					continue;
				Release& rel = rels[k];

				rel.id = Guid::parse(json_object_get_string(obj, "id"));
				rel.artist = ParseArtists(obj);
				rel.title.assignNFC(json_object_get_string(obj, "title"));
				const char* s = json_object_get_string(obj, "country");
				if (s)
					strncpy(rel.country, s, sizeof rel.country - 1);
				JsonObject* subobj = json_object_get_object(obj, "date");
				if (subobj)
				{	rel.date.year = json_object_get_int(subobj, "year");
					rel.date.month = json_object_get_int(subobj, "month");
					rel.date.day = json_object_get_int(subobj, "day");
				}

				rel.medium_count = json_object_get_int(obj, "medium_count");
				obj = json_object_get_first_object(obj, "mediums");
				if (!obj)
					continue;
				rel.format.assignNFC(json_object_get_string(obj, "format"));
				rel.medium = json_object_get_int(obj, "position");

				rel.track_count = json_object_get_int(obj, "track_count");
				obj = json_object_get_first_object(obj, "tracks");
				if (!obj)
					continue;
				rel.track = json_object_get_int(obj, "position");
				xStringD0 artist = ParseArtists(obj);
				if (artist)
					rec.artist = move(artist);
			}
		}
	}

	// order by score descending
	sort(recordings, recordings + recording_count,
		[](const Recording& x, const Recording& y) { return y.score < x.score; });

done:
	matches.set_recordings(recordings, recording_count);

	return string();
}

void AcoustIDImpl::AnalyzeFile(ET_File& file, GCancellable* cancel)
{
	g_return_if_fail(file.AcoustIDMatches);
	Cancel.reset();

	string result;
	if (file.ETFileInfo.duration <= 0)
	{	result = _("The duration of this file is invalid.");
		goto error;
	}

	if (cancel)
		Cancel = gObject<GCancellable>((GCancellable*)g_object_ref(cancel));

	Duration = file.ETFileInfo.duration;
	result = CalcFingerprint(file.FilePath);
	if (!result.empty())
		goto error;

	result = CallLookup();
	if (!result.empty())
		goto error;

	result = ParseJson(const_cast<Matches&>(*file.AcoustIDMatches));

	json_node_free(JsonResponse);
	JsonResponse = nullptr;

	if (result.empty())
		return;

error:
	if (result != "$Aborted") // state abort is set by the main thread
		const_cast<Matches&>(*file.AcoustIDMatches).set_error(result.c_str());
}

} // namespace

AcoustID* AcoustID::Factory()
{	return new AcoustIDImpl();
}

string AcoustID::Date::toString() const
{	string result;
	if (year)
	{	result += to_string(year);
		if (month && month <= 12)
		{	result += '-';
			result += '0' + month / 10;
			result += '0' + month % 10;
			if (day && day <= 31)
			{	result += '-';
				result += '0' + day / 10;
				result += '0' + day % 10;
			}
		}
	}
	return result;
}

AcoustID::Date AcoustID::Recording::first_release() const
{	Date result;
	for (unsigned i = 0; i < release_count; ++i)
	{	Date release = releases[i].date;
		if (!release.year)
			continue; // ignore release w/o date
		if (!result.year || release.year < result.year)
			result = release;
		else if (release.year > result.year || !release.month)
			continue;
		else if (!result.month || release.month < result.month)
			result = release;
		else if (release.month > result.month || !release.day)
			continue;
		else if (!result.day || release.day < result.day)
			result = release;
	}
	return result;
}

AcoustID::Matches::~Matches()
{	switch (state)
	{case VALID:
		recordings.~unique_ptr();
		break;
	 case ERROR:
		error.~xStringD0();
	}
}

void AcoustID::Matches::set_recordings(const Recording* rec, unsigned count) noexcept
{	assert(state <= ABORTED);
	new (&recordings) unique_ptr<const Recording[]>(rec);
	recording_count = count;
	state = VALID;
}

void AcoustID::Matches::set_error(const char* msg) noexcept
{	assert(state <= ABORTED);
	new (&error) xStringD0(msg);
	state = ERROR;
}

AcoustID::State AcoustID::Matches::abort() noexcept
{	State old = PENDING;
	state.compare_exchange_strong(old, ABORTED);
	return old;
}

AcoustID::State AcoustID::Matches::restart() noexcept
{	State old = ABORTED;
	state.compare_exchange_strong(old, PENDING);
	return old;
}

xPtr<AcoustIDWorker> AcoustIDWorker::Instance;
AcoustIDWorker::FileList AcoustIDWorker::Files;
mutex AcoustIDWorker::Mtx;
void (*AcoustIDWorker::OnFileUpdated)(gpointer userData, ET_File* file, unsigned remaining);
void (*AcoustIDWorker::OnFinished)(gpointer userData, bool cancelled);
gpointer AcoustIDWorker::UserData;

AcoustIDWorker::AcoustIDWorker()
:	Service(AcoustID::Factory())
,	Cancel(g_cancellable_new())
{}

void AcoustIDWorker::Run()
{	xPtr<ET_File> lastfile;
	while (true)
	{	ET_File* file;
		unsigned remaining;
		{	lock_guard<mutex> lock(Mtx);
			if (Files.size())
			{	if (Files.back() == lastfile.get())
					Files.pop_back();
			}
			while (Files.size())
			{	file = Files.back().get();
				if (file->AcoustIDMatches->get_state() == AcoustID::PENDING)
					break;
				Files.pop_back();
			}
			remaining = Files.size();
		}

		if (lastfile && lastfile->AcoustIDMatches->get_state() > AcoustID::ABORTED)
			gIdleAdd(new function<void()>([lastfile = move(lastfile), remaining]()
			{	if (OnFileUpdated)
					(*OnFileUpdated)(UserData, lastfile.get(), remaining);
			}));

		if (!remaining)
			break;

		Service->AnalyzeFile(*file, Cancel.get());
		lastfile = file;
	}

	gIdleAdd(new function<void()>([that = xPtr<AcoustIDWorker>(this), cancelled = Cancel.get()]()
	{	if (Instance != that)
			return; // a new worker has been started to continue.
		Instance.reset();
		if (OnFinished)
			OnFinished(UserData, g_cancellable_is_cancelled(cancelled));
	}));
}

unsigned AcoustIDWorker::Feed(FileList&& files)
{
	vector<const ET_File*> dupes;
	// remove files already done, arm the others
	files.erase(remove_if(files.begin(), files.end(), [&dupes](const xPtr<ET_File>& file)
		{	if (file->AcoustIDMatches)
				switch (const_cast<AcoustID::Matches&>(*file->AcoustIDMatches).restart())
				{default:
					return true;
				 case AcoustID::PENDING:
					dupes.push_back(file.get());
					return false;
				 case AcoustID::ABORTED:
					return false;
				}
			file->AcoustIDMatches.reset(new AcoustID::Matches());
			return false;
		}), files.end());

	if (files.size() == 0)
		return 0; // all files already done

	// reverse order for use in Files
	reverse(files.begin(), files.end());

	sort(dupes.begin(), dupes.end());
	auto isdupe = [&dupes](const ET_File* file)
	{	return binary_search(dupes.begin(), dupes.end(), file);
	};

	{	lock_guard<mutex> lock(Mtx);
		if (Files.size())
		{	// Check, if file currently in progress is a duplicate.
			if (isdupe(Files.back()))
			{	files.erase(find(files.begin(), files.end(), Files.back()));
				if (files.empty())
					return 0;
			}
			// remove duplicates
			Files.erase(remove_if(Files.begin() + 1, Files.end(), isdupe), Files.end());
			// merge lists
			Files.insert(Files.end() - 1, make_move_iterator(files.begin()), make_move_iterator(files.end()));
			goto done;
		} else
			Files.swap(files);
	}

	// Files.size() was 0 => need to start new thread
	Instance = new AcoustIDWorker();
	std::thread(&AcoustIDWorker::Run, ref(*Instance)).detach();
done:
	return Files.size();
}

bool AcoustIDWorker::Stop()
{
	FileList files;
	{	lock_guard<mutex> lock(Mtx);
		files.swap(Files);
	}

	if (!files.size())
		return false;

	if (Instance)
		g_cancellable_cancel(Instance->Cancel.get());

	for (const auto& file : files)
		if (const_cast<AcoustID::Matches&>(*file->AcoustIDMatches).abort() == AcoustID::PENDING)
			(*OnFileUpdated)(UserData, file.get(), 0);

	return true;
}


#endif
