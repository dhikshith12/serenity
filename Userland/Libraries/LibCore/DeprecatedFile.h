/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/Error.h>
#include <LibCore/IODevice.h>
#include <sys/stat.h>

namespace Core {

///
/// Use of Core::File for reading/writing data is deprecated.
/// Please use Core::File and Core::InputBufferedFile instead.
///
class DeprecatedFile final : public IODevice {
    C_OBJECT(DeprecatedFile)
public:
    virtual ~DeprecatedFile() override;

    static ErrorOr<NonnullRefPtr<DeprecatedFile>> open(DeprecatedString filename, OpenMode, mode_t = 0644);

    DeprecatedString filename() const { return m_filename; }

    bool is_directory() const;
    bool is_device() const;
    bool is_block_device() const;
    bool is_char_device() const;
    bool is_link() const;

    static DeprecatedString current_working_directory();
    static DeprecatedString absolute_path(DeprecatedString const& path);

    static DeprecatedString real_path_for(DeprecatedString const& filename);

    virtual bool open(OpenMode) override;

    enum class ShouldCloseFileDescriptor {
        No = 0,
        Yes
    };
    bool open(int fd, OpenMode, ShouldCloseFileDescriptor);
    [[nodiscard]] int leak_fd();

    static Optional<DeprecatedString> resolve_executable_from_environment(StringView filename);

private:
    DeprecatedFile(Object* parent = nullptr)
        : IODevice(parent)
    {
    }
    explicit DeprecatedFile(DeprecatedString filename, Object* parent = nullptr);

    bool open_impl(OpenMode, mode_t);

    DeprecatedString m_filename;
    ShouldCloseFileDescriptor m_should_close_file_descriptor { ShouldCloseFileDescriptor::Yes };
};

}
