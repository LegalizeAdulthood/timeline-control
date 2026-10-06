// Copyright (c) 2026 Richard Thomson

#include <timelineViewer/write_snapshot.h>

#include <timeline/size_cast.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace
{

/// Snapshot output path isolated across viewer I/O test cases.
///
class ViewerIoSnapshot : public testing::Test
{
protected:
    void SetUp() override
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    const std::filesystem::path m_path{std::filesystem::current_path() / "timeline-viewer-io-test.txt"};
};

TEST_F(ViewerIoSnapshot, writesSnapshot)
{
    const std::string expected = "timeline snapshot\n";

    const timeline_viewer::SnapshotWriteResult result = timeline_viewer::write_snapshot(m_path, expected);
    std::ifstream input(m_path, std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    ASSERT_TRUE(result.succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(expected, actual);
}

TEST_F(ViewerIoSnapshot, reportsWriteFailure)
{
    const std::filesystem::path path = std::filesystem::current_path() / "timeline-viewer-io-absent" / "snapshot.txt";

    const timeline_viewer::SnapshotWriteResult result = timeline_viewer::write_snapshot(path, "snapshot");

    EXPECT_FALSE(result.succeeded());
    ASSERT_EQ(1, timeline::size_cast(result.diagnostics));
    EXPECT_EQ("Unable to write the timeline snapshot.", result.diagnostics.front());
}

} // namespace
