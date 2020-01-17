#pragma once
#include "core/se_string.h"
#include "core/io/logger.h"

class FileAccess;
/**
 * Writes messages to the specified file. If the file already exists, creates a copy (backup)
 * of it with timestamp appended to the file name. Maximum number of backups is configurable.
 * When maximum is reached, the oldest backups are erased. With the maximum being equal to 1,
 * it acts as a simple file logger.
 */
class RotatedFileLogger : public Logger {
    String base_path;
    int max_files;

    FileAccess *file;

    void rotate_file_without_closing();
    void close_file();
    void clear_old_backups() const;
    void rotate_file();

public:
    RotatedFileLogger(const String &p_base_path, int p_max_files = 10);

    void logv(se_string_view p_msg, bool p_err) override;
    ~RotatedFileLogger() override;
};
