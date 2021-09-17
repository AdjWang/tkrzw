/*************************************************************************************************
 * Message queue on the file stream
 *
 * Copyright 2020 Google LLC
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License.  You may obtain a copy of the License at
 *     https://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific language governing permissions
 * and limitations under the License.
 *************************************************************************************************/

#ifndef _TKRZW_MESSAGE_QUEUE_H
#define _TKRZW_MESSAGE_QUEUE_H

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cinttypes>

#include "tkrzw_file.h"
#include "tkrzw_lib_common.h"

namespace tkrzw {

class MessageQueueImpl;
class MessageQueueReaderImpl;

/**
 * Message queue on the file stream.
 */
class MessageQueue final {
 public:
  /**
   * Messsage reader.
   */
  class Reader final {
    friend class MessageQueue;
   public:
    /**
     * Destructor.
     */
    ~Reader();

    /**
     * Reads a message from the queue.
     * @param timeout The timeout to wait for in seconds.  Zero means no wait.  Negative means
     * unlimited.
     * @param timestamp The pointer to a variable to store the timestamp in milliseconds of the
     * message.
     * @param message The pointer to a string object to store the msssage data.
     * @return The result status.  If the queue is in the read-only mode and there's no record to
     * read, NOT_FOUND_ERROR is returned.  If the time out is reached, INFEASIBLE_ERROR is
     * returned.  If the writer closes the file while waiting, CANCELED_ERROR is returned.
     */
    Status Read(double timeout, int64_t* timestamp, std::string* message);

    /**
     * Gets the latest timestamp.
     * @return The latest timestamp, or -1 if nothing has been read.
     */
    int64_t GetTimestamp();

   private:
    /**
     * Constructor.
     * @param queue_impl The queue implementation object.
     * @param min_timestamp The minimum timestamp in milliseconds of messages to read.
     */
    Reader(MessageQueueImpl* queue_impl, uint64_t min_timestamp);

    /** Pointer to the actual implementation. */
    MessageQueueReaderImpl* impl_;
  };

  /**
   * Enumeration of options for Open.
   */
  enum OpenOption : int32_t {
    /** The default behavior. */
    OPEN_DEFAULT = 0,
    /** To truncate the file. */
    OPEN_TRUNCATE = 1 << 0,
    /** To do physical synchronization for each output. */
    OPEN_SYNC_HARD = 1 << 1,
    /** To not write anything. */
    OPEN_READ_ONLY = 1 << 2,
  };

  /**
   * Default constructor.
   */
  MessageQueue();

  /**
   * Destructor.
   */
  ~MessageQueue();

  /**
   * Opens the queue files to store the messages.
   * @param prefix The prefix for the file names.  The actual name of each file has the suffix
   * of ten digits of the decimal ID, like "0000000000" and "0000123456".
   * @param max_file_size The maximum size of each file.  When the actual file exceeds the limit,
   * a new file is created and new messages are written into it.
   * @param options Bit-sum options of MessageQueue::OpenOption enums.
   * @return The result status.
   */
  Status Open(const std::string& prefix, int64_t max_file_size, int32_t options = OPEN_DEFAULT);

  /**
   * Closes the queue files.
   * @return The result status.
   */
  Status Close();

  /**
   * Writes a message.
   * @param timestamp The timestamp in milliseconds of the massage.  If it is negative, the
   * current wall time is specified.
   * @param message The message data.
   * @return The result status.
   */
  Status Write(int64_t timestamp, std::string_view message);

  /**
   * Synchronizes the metadata and content to the file system.
   * @param hard True to do physical synchronization with the hardware or false to do only
   * logical synchronization with the file system.
   * @return The result status.
   * @details The metadata of the current file size is not written to the file until this method
   * is called.  When an external process reads the latest file, it reads the region only to the
   * offset of the metadata file size.  If OPEN_SYNC_HARD is set to the options of the Open
   * method, synchronization is done implicitly for each write.
   */
  Status Synchronize(bool hard);

  /**
   * Gets the latest timestamp.
   * @return The latest timestamp, or -1 on failure.
   */
  int64_t GetTimestamp();

  /**
   * Makes a message reader.
   * @param min_timestamp The minimum timestamp in milliseconds of messages to read.
   * @return The message reader.
   */
  std::unique_ptr<Reader> MakeReader(int64_t min_timestamp);

  /**
   * Finds files matching the given prefix and the date format suffix.
   * @param prefix The prefix for the file names.
   * @param paths The pointer to a vector object to store the result paths.
   * @return The result status.  Even if there's no matching file, SUCCESS is returned.
   * @details The matched paths are sorted in ascending order of the file names.
   */
  static Status FindFiles(const std::string& prefix, std::vector<std::string>* paths);

  /**
   * Gets the ID number of a message file path.
   * @param path The path of the message file.
   * @return The ID number of the message file.
   */
  static uint64_t GetFileID(const std::string& path);

  /**
   * Reads the metadata of a message file path.
   * @param path The path of the message file.
   * @param file_id The pointer to a variable to store the file ID.
   * @param timestamp The pointer to a variable to store the timestamp in milliseconds.  The
   * timestamp represents the newest record in the file.
   * @param file_size The pointer to a variable to store the file size.
   * @return The result status.
   */
  static Status ReadFileMetadata(
      const std::string& path, int64_t *file_id, int64_t* timestamp, int64_t* file_size);

  /**
   * Remove files whose newest message is older than a threshold.
   * @param prefix The prefix for the file names.
   * @param threshold The threshold timestamp in milliseconds.
   * @return The result status.  Even if no files are matched, it returns success.
   */
  static Status RemoveOldFiles(const std::string& prefix, int64_t threshold);

  /**
   * Reads the next message from a file.
   * @param file The file object to read from.
   * @param file_offset The pointer to the variable containing the offset to read and to store
   * the offset of the next record.  If the initial value can be zero to read the first record.
   * @param timestamp The pointer to a variable to store the timestamp in milliseconds of the
   * message.
   * @param min_timestamp The minimum timestamp in milliseconds to fill the message data.
   * @param message The pointer to a string object to store the msssage data.
   * @return The result status.  If the remaining part from the offset is filled with null codes,
   * CANCELED_ERROR is returned.
   */
  static Status ReadNextMessage(
      File* file, int64_t* file_offset, int64_t* timestamp, std::string* message,
      int64_t min_timestamp = 0);

 private:
  /** Pointer to the actual implementation. */
  MessageQueueImpl* impl_;
};

}  // namespace tkrzw

#endif  // _TKRZW_MESSAGE_QUEUE_H

// END OF FILE
