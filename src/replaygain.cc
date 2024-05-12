/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2024  Marcel Mueller <github@maazl.de>
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

#include "replaygain.h"
#ifdef ENABLE_REPLAYGAIN

#include "misc.h"
#include "setting.h"

#include <vector>
#include <memory>
#include <cmath>
#include <limits>
#include <algorithm>

#include <glib/gi18n.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

using namespace std;


static constexpr int SampleRate = 48000;


class ReplayGain : public ReplayGainAnalyzer::Result
{
protected:
	const int BlockSize;
	int BlockLevel;
	int Channels = 0;
	float Maximum = 0;
	virtual void Feed(int channel, const float* data, int count) = 0;
	virtual void ProcessBlock() = 0;
public:
	ReplayGain(int blockSize) : BlockSize(blockSize), BlockLevel(blockSize) {}
	virtual void Setup(uint64_t channel_layout) = 0;
	void Feed(const float*const* data, int samples);
	virtual float Peak() const { return Maximum; }
};

void ReplayGain::Feed(const float*const* data, int samples)
{	float* x[Channels];
	memcpy(x, data, Channels * sizeof(float*));
	while (samples)
	{	// Fragment at BlockSize
		int block = min(BlockLevel, samples);

		for (int ch = 0; ch < Channels; ++ch)
		{	Feed(ch, x[ch], block);
			x[ch] += block;
		}
		samples -= block;

		if ((BlockLevel -= block) == 0)
		{	ProcessBlock();
			BlockLevel = BlockSize;
		}
	}
}

/** ReplayGain version 1 */
class ReplayGain1 : public ReplayGain
{protected:
	static constexpr int Bins4dB = 30;
	static constexpr int dBRange = 60;
	static constexpr float PinkRef = -25.5;
	struct Channel
	{	// RMS calculation
		double Sum = 0;
		// filter ring buffers
		bool Xi = 0;
		signed char YZi = 0;
		float X[2] = {0};
		double Y[16] = {0};
		double Z[16] = {0};
		float Gain = 1;
		float Feed(const float* data, int count);
	private:
		float y(signed char i) { return Y[(YZi + i) & 15]; }
		float z(signed char i) { return Z[(YZi + i) & 15]; }
	};
	unique_ptr<Channel[]> ChannelBuf;
	int BlockCount = 0;
	// dB bins for quantile calculation
	int Bins[Bins4dB * dBRange];
protected:
	virtual void Feed(int channel, const float* data, int count);
	virtual void ProcessBlock();
public:
	ReplayGain1(int blockSize = SampleRate * 50 / 1000);
	virtual void Setup(uint64_t channel_layout);
	virtual float Gain() const;
	virtual void operator+=(const Result& r);
};

ReplayGain1::ReplayGain1(int blockSize) : ReplayGain(blockSize)
{	memset(Bins, 0, sizeof(Bins));
}

void ReplayGain1::Setup(uint64_t channel_layout)
{	Channels = 0;
	int center = -1;
	for (uint64_t ch = 1; ch; ch <<= 1)
		if (channel_layout & ch)
		{	if (ch == AV_CH_FRONT_CENTER)
				center = Channels;
			++Channels;
		}

	ChannelBuf.reset(new Channel[Channels]);
	for (int ch = 0; ch < Channels; ++ch)
		if (ch != center)
			ChannelBuf[ch].Gain = .5;
}

void ReplayGain1::Feed(int channel, const float* x, int count)
{	float max = ChannelBuf[channel].Feed(x, count);
	if (max > Maximum)
		Maximum = max;
}

float ReplayGain1::Channel::Feed(const float* x, int count)
{	float max = 0;
	while (count--)
	{	// Peak
		float in = *x++;
		double res;
		if (in > max)
			max = in;
		// Butterworth filter
		Y[YZi] = (X[Xi] + in) * 0.98621192462708
		    + X[!Xi] * -1.97242384925416
		    + y(-2) * -0.97261396931306
		    + y(-1) * 1.97223372919527;
		X[Xi] = in;
		Xi ^= true;
		// Yule filter
		res = y(-10) * 0.00288463683916
		    + y(-9) * 0.00012025322027
		    + y(-8) * 0.00306428023191
		    + y(-7) * 0.00594298065125
		    + y(-6) * -0.02074045215285
		    + y(-5) * 0.02161526843274
		    + y(-4) * -0.01655260341619
		    + y(-3) * -0.00009291677959
		    + y(-2) * -0.00123395316851
		    + y(-1) * -0.02160367184185
		    + Y[YZi] * 0.03857599435200
		    + z(-10) * -0.13919314567432
		    + z(-9) * 0.86984376593551
		    + z(-8) * -2.75465861874613
		    + z(-7) * 5.87257861775999
		    + z(-6) * -9.48293806319790
		    + z(-5) * 12.28759895145294
		    + z(-4) * -13.05504219327545
		    + z(-3) * 11.34170355132042
		    + z(-2) * -7.81501653005538
		    + z(-1) * 3.84664617118067;
		Z[YZi] = res;
		// RMS calculation
		Sum += res * res;
		++YZi &= 15;
	}
	return max;
}

void ReplayGain1::ProcessBlock()
{	double sum = 0;
	for (Channel* cp = ChannelBuf.get(), *cpe = cp + Channels; cp != cpe; ++cp)
	{	sum += cp->Sum * cp->Gain;
		cp->Sum = 0;
	}

	++BlockCount;
	if (!sum)
	{	++Bins[0];
		return;
	}
	long bin = lround(Bins4dB * (10 * log10(sum / BlockSize) - PinkRef + dBRange/2.f));
	if (bin < 0)
		bin = 0;
	else if (bin >= dBRange * Bins4dB)
		bin = dBRange * Bins4dB - 1;
	++Bins[bin];
}

float ReplayGain1::Gain() const
{	if (!BlockCount)
		return numeric_limits<float>::quiet_NaN();
	// find 95% quantile
	int target = (BlockCount + 10) / 20;
	const int* bp = Bins + dBRange * Bins4dB;
	do
		target -= *--bp;
	while (target > 0);
	if (target < bp[1] - target)
		++bp;
	return dBRange / 2 - (double)(bp - Bins) / Bins4dB;
}

void ReplayGain1::operator+=(const Result& right)
{	const ReplayGain1& rg = dynamic_cast<const ReplayGain1&>(right);
	if (rg.Maximum > Maximum)
		Maximum = rg.Maximum;
	BlockCount += rg.BlockCount;
	int* dp = Bins;
	for (int v : rg.Bins)
		*dp++ += v;
}

/** ReplayGain version 1.5 hybrid */
class ReplayGain15 : public ReplayGain1
{	double Sums[3] = {0};
protected:
	static constexpr float PinkRef = -26.5;
	virtual void ProcessBlock();
public:
	ReplayGain15() : ReplayGain1(SampleRate * 100 / 1000) {}
};

void ReplayGain15::ProcessBlock()
{	double sum = 0;
	for (Channel* cp = ChannelBuf.get(), *cpe = cp + Channels; cp != cpe; ++cp)
	{	sum += cp->Sum * cp->Gain;
		cp->Sum = 0;
	}

	double sum4 = Sums[2];
	sum4 += Sums[2] = Sums[1];
	sum4 += Sums[1] = Sums[0];
	sum4 += Sums[0] = sum;

	if (!Sums[2])
		return; // not yet 4 buffers processed

	++BlockCount;
	long bin = lround(Bins4dB * (10 * log10(sum4 / (BlockSize << 2)) - PinkRef + dBRange/2.f));
	if (bin < 0)
		bin = 0;
	else if (bin >= dBRange * Bins4dB)
		bin = dBRange * Bins4dB - 1;
	++Bins[bin];
}

/** ReplayGain version 2 */
class ReplayGain2 : public ReplayGain
{	static constexpr int BlockSize = SampleRate * 400 / 1000;
	struct Channel
	{	// RMS calculation
		float Gain = 1;
		// filter ring buffers
		int XYi = 0;
		double X[2] = {0};
		double Y[2] = {0};
	};
	double Zsum[4] = {0};
	unique_ptr<Channel[]> ChannelBuf;
	double Ljsum = 0;
	vector<float> Lj;
protected:
	virtual void Feed(int channel, const float* data, int count);
	virtual void ProcessBlock();
public:
	ReplayGain2() : ReplayGain(BlockSize / 4), Lj(100) {}
	virtual void Setup(uint64_t channel_layout);
	virtual float Gain() const;
	virtual void operator+=(const Result& r);
};

void ReplayGain2::Setup(uint64_t channel_layout)
{	Channels = 0;
	vector<float> gain;
	gain.reserve(10);
	for (uint64_t ch = 1; ch; ch <<= 1)
		if (channel_layout & ch)
		{	if (ch & (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT|AV_CH_FRONT_CENTER|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER))
				gain.emplace_back(1);
			else if (ch & (AV_CH_LOW_FREQUENCY|AV_CH_LOW_FREQUENCY_2))
				gain.emplace_back(0);
			else
				gain.emplace_back(M_SQRT2);
			++Channels;
		}

	ChannelBuf.reset(new Channel[Channels]);
	for (int ch = 0; ch < Channels; ++ch)
		ChannelBuf[ch].Gain = gain[ch];
}

void ReplayGain2::Feed(int channel, const float* data, int count)
{	g_assert(channel < Channels);
	register Channel& ch = ChannelBuf[channel];
	if (!ch.Gain)
		return;
	register int i = ch.XYi;
	while (count--)
	{	float in = *data++;
		if (in > Maximum)
			Maximum = in;
		// Highpass
		register double tmp = in
			+ 1.99004745483398 * ch.X[i]
			- 0.99007225036621 * ch.X[!i];
		register double acc = tmp - 2 * ch.X[i] + ch.X[!i];
		ch.X[!i] = tmp;
		// Head filter
		tmp = acc
			+ 1.69065929318241 * ch.Y[i]
			- 0.73248077421585 * ch.Y[!i];
		acc = 1.53512485958697 * tmp
			- 2.69169618940638 * ch.Y[i]
			+ 1.19839281085285 * ch.Y[!i];
		ch.Y[!i] = tmp;
		// RMS
		Zsum[0] += acc * acc * ch.Gain;
		i = !i;
	}
	ch.XYi = i;
}

void ReplayGain2::ProcessBlock()
{	double sum = Zsum[3];
	sum += Zsum[3] = Zsum[2];
	sum += Zsum[2] = Zsum[1];
	sum += Zsum[1] = Zsum[0];
	Zsum[0] = 0;

	if (Zsum[3])
	{	float lj = 10 * log10(sum / BlockSize) - .691;
		if (lj > -70) // > Γa
		{	Lj.emplace_back(lj);
			Ljsum += lj;
		}
	}
}

float ReplayGain2::Gain() const
{	size_t count = Lj.size();
	double ljsum = Ljsum;
	float gr = ljsum / count - 10;
	for (float lj : Lj)
		if (lj <= gr)
		{	ljsum -= lj;
			--count;
		}
	return -18 - ljsum / count;
}

void ReplayGain2::operator+=(const Result& right)
{	const ReplayGain2& rg = dynamic_cast<const ReplayGain2&>(right);
	if (rg.Maximum > Maximum)
		Maximum = rg.Maximum;
	Ljsum += rg.Ljsum;
	Lj.insert(Lj.end(), rg.Lj.begin(), rg.Lj.end());
}

static ReplayGain* Factory()
{	switch (g_settings_get_enum(MainSettings, "replaygain-model"))
	{case ET_REPLAYGAIN_MODEL_V1:
		return new ReplayGain1();
	 case ET_REPLAYGAIN_MODEL_V2:
		return new ReplayGain2();
	 case ET_REPLAYGAIN_MODEL_V15:
		return new ReplayGain15();
	 default:
		g_assert_not_reached();
	}
}

string ReplayGainAnalyzer::AnalyzeFile(const char* fileName)
{
	char err[AV_ERROR_MAX_STRING_SIZE];
	auto avstrerr = [&err](int e) { return (const char*)av_make_error_string(&err[0], sizeof(err), e); };

	// get format from audio file
	AVFormatContext* pformat = nullptr;
	int rc = avformat_open_input(&pformat, fileName, nullptr, nullptr);
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
	auto swr = make_unique(swr_alloc(), [](SwrContext* ptr) { swr_free(&ptr); });
	av_opt_set_int(swr.get(), "in_channel_layout", codec->channel_layout, 0);
	av_opt_set_int(swr.get(), "out_channel_layout", codec->channel_layout, 0);
	av_opt_set_int(swr.get(), "in_sample_rate", codec->sample_rate, 0);
	av_opt_set_int(swr.get(), "out_sample_rate", SampleRate, 0);
	av_opt_set_sample_fmt(swr.get(), "in_sample_fmt", codec->sample_fmt, 0);
	av_opt_set_sample_fmt(swr.get(), "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	rc = swr_init(swr.get());
	if (rc < 0)
		return string(_("Resampler has not been properly initialized: ")) + avstrerr(rc);

	ReplayGain* acc = Factory();
	Last.reset(acc);
	acc->Setup(codec->channel_layout);

	// prepare to read data
	auto frame = make_unique(av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); });

	float* buffer[codec->channels];
	int buffersize = 0;
	auto buffer_ptr = make_unique((float*)nullptr, [](float* ptr) { av_freep(&ptr); });

	// iterate through frames
	AVPacket packet{0};
	bool done = false;
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

		while (avcodec_receive_frame(codec.get(), frame.get()) == 0)
		{
			// resample
			int outsamples = swr_get_delay(swr.get(), SampleRate)
				+ av_rescale_rnd(frame->nb_samples, SampleRate, frame->sample_rate, AV_ROUND_UP);

			if (outsamples > buffersize)
			{	buffer_ptr.reset();
				rc = av_samples_alloc((uint8_t**)buffer, NULL, codec->channels, outsamples, AV_SAMPLE_FMT_FLTP, 0);
				if (rc < 0)
					return string("Memory allocation error: ") + avstrerr(rc);
				buffer_ptr.reset(buffer[0]);
				buffersize = outsamples;
			}

			count = swr_convert(swr.get(), (uint8_t**)&buffer, buffersize, (const uint8_t**)frame->extended_data, frame->nb_samples);
			if (count < 0)
				return string("Sample rate conversion failed: ") + avstrerr(rc);

			// and process
			acc->Feed(buffer, count);
		}

	} while (!done);
	// flush resampler
	count = swr_convert(swr.get(), (uint8_t**)&buffer, buffersize, nullptr, 0);
	if (count > 0)
		acc->Feed(buffer, count);

	// album gain
	if (!Aggregated)
		Aggregated.reset(Factory());
	*Aggregated += *acc;

	// all cleanup is done by unique_ptr destructors
	return string();
}

#endif
