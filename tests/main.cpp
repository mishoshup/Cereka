#include <gtest/gtest.h>

#ifdef _WIN32
int wmain(int argc,
          wchar_t *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#else
int main(int argc,
         char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
