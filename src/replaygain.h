/* EasyTAG - tag editor for audio files
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

#ifndef ET_REPLAYGAIN_H_
#define ET_REPLAYGAIN_H_

#include "config.h"
#ifdef ENABLE_REPLAYGAIN

#include <string>
#include <memory>

class ReplayGainAnalyzer
{
public:
	struct Result
	{	virtual ~Result() {}
		/** Recommended gain adjust in dB. */
		virtual float Gain() const = 0;
		/** Peak amplitude relative to FSR */
		virtual float Peak() const = 0;
		/** aggregate results */
		virtual void operator+=(const Result& r) = 0;
	};
private:
	std::unique_ptr<Result> Last;
	std::unique_ptr<Result> Aggregated;
public:
	/** Analyze a file and aggregate the results in the current instance.
	 * @param fileName
	 * @return Error message if any, empty on success. */
	std::string AnalyzeFile(const char* fileName);

	/** Retrieve the last file result (for track gain). */
	const Result& GetLastResult() const { return *Last; }

	/** Retrieve the current aggregated result (for album gain). */
	const Result& GetAggregatedResult() const { return *Aggregated; }

	/** Reset state of the analyzer, i.e. discard results. */
	void Reset() { Last.reset(); Aggregated.reset(); }
};

#endif
#endif /* !ET_SCAN_H_ */
