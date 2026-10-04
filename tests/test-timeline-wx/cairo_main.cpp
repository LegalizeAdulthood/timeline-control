// Copyright (c) 2026 Richard Thomson

#include <gtest/gtest.h>

#include <wx/app.h>

/// Native application lifetime for Cairo renderer and wx control checks.
///
class CairoTestApp : public wxApp
{
public:
    bool OnInit() override
    {
        return true;
    }
};

wxIMPLEMENT_APP_NO_MAIN(CairoTestApp);

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    if (testing::GTEST_FLAG(list_tests))
    {
        return RUN_ALL_TESTS();
    }
    if (!wxEntryStart(argc, argv))
    {
        return 1;
    }
    if (!wxTheApp->CallOnInit())
    {
        wxEntryCleanup();
        return 1;
    }
    const int result = RUN_ALL_TESTS();
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
