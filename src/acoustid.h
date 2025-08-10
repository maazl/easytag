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

#ifndef ET_ACOUSTID_H_
#define ET_ACOUSTID_H_

#include "config.h"
#ifdef ENABLE_ACOUSTID
#include "misc.h"
#include "xstring.h"
#include "xptr.h"

#include <string>
#include <memory>
#include <vector>
#include <mutex>

class ET_File;

/// Call the AcoustID lookup service with fingerprint of files.
class AcoustID
{
public:
	struct Date
	{	unsigned year : 14;
		unsigned month : 4;
		unsigned day : 5;
		constexpr Date() noexcept : year(0), month(0), day(0) {}
		std::string toString() const;
	};
	struct Release
	{	Guid id;
		xStringD0 artist;
		xStringD0 title;
		xStringD0 format;
		char country[4] = {0};
		Date date;
		unsigned medium_count = 0;
		unsigned medium = 0;
		unsigned track_count = 0;
		unsigned track = 0;
	};
	struct Recording
	{	Guid id = Guid::empty;
		xStringD0 artist;
		xStringD0 title;
		double duration = 0.;
		float score = 0.;
		unsigned release_count = 0;
		std::unique_ptr<const Release[]> releases;
		Date first_release() const;
	};
	enum State : unsigned char
	{	PENDING,
		ABORTED,
		// Final states from here
		VALID,
		ERROR
	};
	class Matches
	{	union
		{	std::unique_ptr<const Recording[]> recordings;
			xStringD0 error;
		};
		unsigned recording_count;
		std::atomic<State> state;
	public:
		Matches() noexcept : recording_count(0), state(PENDING) {}
		~Matches();
		State get_state() const noexcept { return state; }
		unsigned get_recording_count() const noexcept { return state == VALID ? recording_count : 0; }
		const Recording* get_first_recording() const noexcept { return state == VALID ? recordings.get() : nullptr; }
		const char* get_error() const noexcept { return state == ERROR ? error.get() : nullptr; }
		void set_recordings(const Recording* rec, unsigned count) noexcept;
		void set_error(const char* error) noexcept;
		State abort() noexcept;
		State restart() noexcept;
	};
protected:
	AcoustID() noexcept {}
public:
	/// Create instance
	static AcoustID* Factory();
	virtual ~AcoustID() {}
	/// Analyze a file.
	/// @param fileName
	/// @details The result is stored in file.AcoustIDMatches.
	virtual void AnalyzeFile(ET_File& file, GCancellable* cancel = nullptr) = 0;
};

class AcoustIDWorker : public xObj
{
	typedef std::vector<xPtr<ET_File>> FileList;

	/// The currently running instance if any. [UI thread]
	static xPtr<AcoustIDWorker> Instance;
	/// List of files to process. [synchronized]
	/// @details The files are processed back to front.\n
	/// Once the list is empty the worker terminates.
	/// But in between the set of files may be changed or reordered.
	static FileList Files;
	/// Synchronization
	static std::mutex Mtx;

	const std::unique_ptr<AcoustID> Service;
	gObject<GCancellable> Cancel;

private:
	/// AcoustIDMatches of file updated. [unsynchronized]
	static void (*OnFileUpdated)(gpointer userData, ET_File* file, unsigned remaining);
	/// Worker thread terminated [unsynchronized]
	static void (*OnFinished)(gpointer userData, bool cancelled);
	static gpointer UserData;

	AcoustIDWorker();
	/// Background thread
	void Run();
public:
	/// Setup event handlers
	/// @param onFileCompleted Called each time the AcoustIDMatches of file are updated.
	/// @param onFinished Called when the worker finished its work.
	/// All event handlers are called from UI thread.
	/// @remarks This function must not be called while the worker is running.
	template <typename T>
	static void RegisterEvents(void (*onFileUpdated)(T* userData, ET_File* file, unsigned remaining), void (*onFinished)(T* userData, bool cancelled), T* userData)
	{	OnFileUpdated = (void (*)(gpointer, ET_File*, unsigned))onFileUpdated;
		OnFinished = (void (*)(gpointer, bool))onFinished;
		UserData = userData;
	}
	/// Schedule files for requesting AcoustID data.
	/// @return Number of remaining files to process.
	/// @details - All files passed with a final state are ignored.
	/// - The remaining files are set to state PENDING and are scheduled for fingerprinting.
	/// - The files to process are added at the front of the queue.
	/// - If no worker is currently running a new one is started.
	static unsigned Feed(FileList&& files);
	/// Terminate the current worker ASAP. [UI thread]
	/// @return At lease one file has been cancelled.
	/// @details This immediately cancels the processing of the current file
	/// and sets all files in the queue to state ABORTED.
	/// The latter causes a bunch of calls to the onFileCompleted event.
	/// The function in general returns before the worker has terminated.
	/// There is also a small risk that a file that has been just cancelled
	/// will eventually enter a final state shortly.
	static bool Stop();
};

#endif
#endif // ET_ACOUSTID_H_
