/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2013 Synergy Si Ltd.
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "synergy/StreamChunker.h"

#include "synergy/FileChunk.h"
#include "synergy/ClipboardChunk.h"
#include "synergy/protocol_types.h"
#include "base/EventTypes.h"
#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/EventTypes.h"
#include "base/Log.h"
#include "base/Stopwatch.h"
#include "base/String.h"
#include "common/stdexcept.h"

#include <fstream>

#define PAUSE_TIME_HACK 0.1

using namespace std;

const size_t StreamChunker::m_chunkSize = 512 * 1024; // 512kb

void
StreamChunker::sendFile(
				char* filename,
				IEventQueue* events,
				void* eventTarget)
{
	std::fstream file(reinterpret_cast<char*>(filename), std::ios::in | std::ios::binary);

	if (!file.is_open()) {
		throw runtime_error("failed to open file");
	}

	// check file size
	file.seekg (0, std::ios::end);
	size_t size = (size_t)file.tellg();

	// send first message (file size)
	String fileSize = synergy::string::sizeTypeToString(size);
	FileChunk* sizeMessage = FileChunk::start(fileSize);

	events->addEvent(Event(events->forIScreen().fileChunkSending(), eventTarget, sizeMessage));

	// send chunk messages with a fixed chunk size
	size_t sentLength = 0;
	size_t chunkSize = m_chunkSize;
	Stopwatch stopwatch;
	stopwatch.start();
	file.seekg (0, std::ios::beg);
	while (true) {
		if (stopwatch.getTime() > PAUSE_TIME_HACK) {
			// make sure we don't read too much from the mock data.
			if (sentLength + chunkSize > size) {
				chunkSize = size - sentLength;
			}

			char* chunkData = new char[chunkSize];
			file.read(chunkData, chunkSize);
			UInt8* data = reinterpret_cast<UInt8*>(chunkData);
			FileChunk* fileChunk = FileChunk::data(data, chunkSize);
			delete[] chunkData;

			events->addEvent(Event(events->forIScreen().fileChunkSending(), eventTarget, fileChunk));

			sentLength += chunkSize;
			file.seekg (sentLength, std::ios::beg);

			if (sentLength == size) {
				break;
			}

			stopwatch.reset();
		}
	}

	// send last message
	FileChunk* end = FileChunk::end();

	events->addEvent(Event(events->forIScreen().fileChunkSending(), eventTarget, end));

	file.close();
}

void
StreamChunker::sendClipboard(
				String& data,
				size_t size,
				ClipboardID id,
				UInt32 sequence,
				IEventQueue* events,
				void* eventTarget)
{
	// send first message (data size)
	String dataSize = synergy::string::sizeTypeToString(size);
	ClipboardChunk* sizeMessage = ClipboardChunk::start(id, sequence, dataSize);
	
	events->addEvent(Event(events->forClipboard().clipboardSending(), eventTarget, sizeMessage));

	// send clipboard chunk with a fixed size
	// TODO: 4096 fails and this shouldn't a magic number
	size_t sentLength = 0;
	size_t chunkSize = 2048;
	Stopwatch stopwatch;
	stopwatch.start();
	while (true) {
		if (stopwatch.getTime() > 0.1f) {
			// make sure we don't read too much from the mock data.
			if (sentLength + chunkSize > size) {
				chunkSize = size - sentLength;
			}

			String chunk(data.substr(sentLength, chunkSize).c_str(), chunkSize);
			ClipboardChunk* dataChunk = ClipboardChunk::data(id, sequence, chunk);
			
			events->addEvent(Event(events->forClipboard().clipboardSending(), eventTarget, dataChunk));

			sentLength += chunkSize;

			if (sentLength == size) {
				break;
			}

			stopwatch.reset();
		}
	}

	// send last message
	ClipboardChunk* end = ClipboardChunk::end(id, sequence);

	events->addEvent(Event(events->forClipboard().clipboardSending(), eventTarget, end));
}
