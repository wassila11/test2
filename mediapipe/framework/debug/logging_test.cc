#include "mediapipe/framework/debug/logging.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "HalideBuffer.h"
#include "absl/base/log_severity.h"
#include "absl/log/absl_check.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "mediapipe/framework/deps/file_path.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/tensor.h"
#include "mediapipe/framework/port/gmock.h"
#include "mediapipe/framework/port/gtest.h"
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/status_matchers.h"
#include "mediapipe/framework/tool/test_util.h"

namespace mediapipe::debug {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;

constexpr char kColorTerm[] = "COLORTERM";

constexpr absl::string_view kTestDataPath =
    "mediapipe/framework/debug/testdata";

constexpr absl::string_view kTestImageFilename = "sergey.png";

MATCHER_P(HasConsecutiveLines, expected,
          absl::StrCat("contains consecutive lines\n", expected)) {
  std::vector<absl::string_view> expected_lines =
      absl::StrSplit(expected, '\n', absl::SkipEmpty());
  ABSL_CHECK(!expected_lines.empty());

  std::vector<std::string> log_lines = absl::StrSplit(arg, '\n');
  for (int n = 0; n < log_lines.size(); ++n) {
    // Find first expected line.
    if (absl::StrContains(log_lines[n], expected_lines[0])) {
      // Check if the following lines match.
      for (int k = 1; k < expected_lines.size(); ++k) {
        if (n + k >= log_lines.size() ||
            !absl::StrContains(log_lines[n + k], expected_lines[k])) {
          return false;
        }
        return true;
      }
    }
  }
  return false;
}

template <typename T>
Tensor::ElementType GetElementType() {
  if constexpr (std::is_same_v<T, float>) {
    return Tensor::ElementType::kFloat32;
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    return Tensor::ElementType::kUInt8;
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return Tensor::ElementType::kInt32;
  } else {
    static_assert(std::is_void_v<T>, "Unsupported type");
  }
}

template <typename T>
Tensor MakeTensor(int width, int height, int num_channels) {
  Tensor tensor(GetElementType<T>(), {1, height, width, num_channels});
  auto view = tensor.GetCpuWriteView();
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      T* data = view.buffer<T>() + (y * width + x) * num_channels;
      double dx = x + 0.5;
      double dy = y + 0.5;
      if (num_channels >= 1) {
        data[0] = static_cast<T>(dx / width);
      }
      if (num_channels >= 2) {
        data[1] = static_cast<T>(dy / height);
      }
      if (num_channels >= 3) {
        data[2] = static_cast<T>((dx + dy) / static_cast<T>(width + height));
      }
      for (int c = 3; c < num_channels; ++c) {
        data[c] =
            static_cast<T>(dx + height - dy) / static_cast<T>(width + height);
      }
    }
  }
  return tensor;
}

class LoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(log_, Log(absl::LogSeverity::kInfo, _, _))
        .WillRepeatedly(Invoke(
            [&](auto severity, const std::string& file_path,
                const std::string& message) { log_lines_ += message + "\n"; }));
    log_.StartCapturingLogs();

    // Disable color output.
    prev_colorterm_env_ = std::getenv(kColorTerm);
    setenv(kColorTerm, "invalid", 1);
  }

  void TearDown() override {
    // Restore the previous environment variable.
    if (prev_colorterm_env_ != nullptr) {
      setenv(kColorTerm,
             prev_colorterm_env_ != nullptr ? prev_colorterm_env_ : "", 1);
    }
  }

  void ExpectLog(absl::string_view message) {}

  const std::string& log_lines() const { return log_lines_; };
  absl::ScopedMockLog& log() { return log_; }

 private:
  absl::ScopedMockLog log_;
  std::string log_lines_;
  char* prev_colorterm_env_ = nullptr;
};

TEST_F(LoggingTest, LogTensorWithNamePrintsNameAndSize) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/2, /*height=*/3, /*num_channels=*/4);
  debug::LogTensor(tensor, "Karlheinz");
  EXPECT_THAT(log_lines(), HasSubstr("Karlheinz[1 3 2 4]"));
  EXPECT_THAT(log_lines(), HasSubstr("\u2551 Karlheinz"));
}

TEST_F(LoggingTest, LogTensorWithoutNamePrintsDefaultNameAndSize) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/2, /*height=*/3, /*num_channels=*/4);
  debug::LogTensor(tensor);
  EXPECT_THAT(log_lines(), HasSubstr("tensor[1 3 2 4]"));
  EXPECT_THAT(log_lines(), HasSubstr("\u2551 tensor"));
}

TEST_F(LoggingTest, LogTensorWithOneChannel) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/20, /*height=*/15, /*num_channels=*/1);
  debug::LogTensor(tensor);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
  ..::--==++**##%%@@
  ..::--==++**##%%@@
  ..::--==++**##%%@@
  ..::--==++**##%%@@
  ..::--==++**##%%@@
  ..::--==++**##%%@@
  ..::--==++**##%%@@)"));
}

TEST_F(LoggingTest, LogTensorWithTwoChannels) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/10, /*height=*/15, /*num_channels=*/2);
  debug::LogTensor(tensor);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
 ..::--==+
..::--==++
::--==++**
:--==++**#
--==++**##
==++**##%%
=++**##%%@)"));
}

TEST_F(LoggingTest, LogTensorWithThreeChannels) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/40, /*height=*/40, /*num_channels=*/3);
  debug::LogTensor(tensor);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
       ........::::::::--------========+
     ........::::::::--------========+++
   ........::::::::--------========+++++
 ........::::::::--------========+++++++
.......::::::::--------========++++++++*
.....::::::::--------========++++++++***
...::::::::--------========++++++++*****
.::::::::--------========++++++++*******
:::::::--------========++++++++********#
:::::--------========++++++++********###
:::--------========++++++++********#####
:--------========++++++++********#######
-------========++++++++********########%
-----========++++++++********########%%%
---========++++++++********########%%%%%
-========++++++++********########%%%%%%%
=======++++++++********########%%%%%%%%@
=====++++++++********########%%%%%%%%@@@
===++++++++********########%%%%%%%%@@@@@
=++++++++********########%%%%%%%%@@@@@@@)"));
}

TEST_F(LoggingTest, LogTensorWithFourChannels) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/60, /*height=*/10, /*num_channels=*/4);
  debug::LogTensor(tensor);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
    ........:::::::::---------=========+++++++++*********###
........:::::::::---------=========++++++++*********########
...:::::::::---------=========+++++++++*********########%%%%
::::::::---------=========++++++++*********#########%%%%%%%%
::::--------=========+++++++++*********#########%%%%%%%%%@@@)"));
}

// Fuck it, we're doing 5 channels!
TEST_F(LoggingTest, LogTensorWithFiveChannels) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/10, /*height=*/10, /*num_channels=*/5);
  debug::LogTensor(tensor);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
::--==++**
:--==++**#
:--==++**#
:--==++**#
--==++**##)"));
}

TEST_F(LoggingTest, DownsamplesLargeTensors) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/1000, /*height=*/1000, /*num_channels=*/1);
  debug::LogTensor(tensor, "tonsir");
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@
            ............::::::::::::------------============++++++++++++************############%%%%%%%%%%%%@@@@@@@@@@@@)"));
}

TEST_F(LoggingTest, LogTensorChannelWithNamePrintsNameAndSize) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/2, /*height=*/3, /*num_channels=*/4);
  debug::LogTensorChannel(tensor, 2, "Hansrainer");
  EXPECT_THAT(log_lines(), HasSubstr("Hansrainer[1 3 2 4], channel 2 ="));
}

TEST_F(LoggingTest, LogTensorChannelWithoutNamePrintsDefaultNameAndSize) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/2, /*height=*/3, /*num_channels=*/4);
  debug::LogTensorChannel(tensor, 2);
  EXPECT_THAT(log_lines(), HasSubstr("tensor[1 3 2 4], channel 2 ="));
}

TEST_F(LoggingTest, LogTensorChannel) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/10, /*height=*/10, /*num_channels=*/2);

  debug::LogTensorChannel(tensor, 0);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
 .:-=+*#%@
 .:-=+*#%@
 .:-=+*#%@
 .:-=+*#%@
 .:-=+*#%@"));

  debug::LogTensorChannel(tensor, 1);
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
::::::::::
==========
**********
%%%%%%%%%%)"));
}

TEST_F(LoggingTest, LogTensorChannelWithOutOfBoundsChannelFails) {
  EXPECT_CALL(log(), Log(absl::LogSeverity::kWarning, _,
                         HasSubstr("cannot log channel")));
  Tensor tensor =
      MakeTensor<float>(/*width=*/10, /*height=*/10, /*num_channels=*/2);
  debug::LogTensorChannel(tensor, 2);
}

TEST_F(LoggingTest, LogTensorWithBadDimensionsFails) {
  EXPECT_CALL(log(), Log(absl::LogSeverity::kWarning, _,
                         HasSubstr("cannot log tensor with shape")));
  Tensor tensor(Tensor::ElementType::kFloat32, {1, 2, 3});
  debug::LogTensor(tensor);
}

TEST_F(LoggingTest, LogTensorWithBadElementTypeFails) {
  EXPECT_CALL(log(), Log(absl::LogSeverity::kWarning, _,
                         HasSubstr("cannot log tensor of type")));
  Tensor tensor =
      MakeTensor<int>(/*width=*/10, /*height=*/10, /*num_channels=*/2);
  debug::LogTensor(tensor);
}

TEST_F(LoggingTest, LogTensorColor) {
  Tensor tensor =
      MakeTensor<float>(/*width=*/4, /*height=*/4, /*num_channels=*/2);

  setenv(kColorTerm, "truecolor", 1);
  debug::LogTensor(tensor);

  // CiderV's terminal actually supports true color, so that the image shows up
  // correctly. Unfortunately, the editor doesn't.
  EXPECT_THAT(log_lines(),
              HasConsecutiveLines(
                  "\x1B[48;2;31;31;0m\x1B[38;2;31;95;0m\xE2\x96\x84\x1B[48;2;"
                  "95;31;0m\x1B[38;2;95;95;0m\xE2\x96\x84\x1B[48;2;159;31;"
                  "0m\x1B[38;2;159;95;0m\xE2\x96\x84\x1B[48;2;223;31;0m\x1B[38;"
                  "2;223;95;0m\xE2\x96\x84\x1B[0m\n"
                  "\x1B[48;2;31;159;0m\x1B[38;2;31;223;0m\xE2\x96\x84\x1B[48;2;"
                  "95;159;0m\x1B[38;2;95;223;0m\xE2\x96\x84\x1B[48;2;159;159;"
                  "0m\x1B[38;2;159;223;0m\xE2\x96\x84\x1B[48;2;223;159;0m\x1B["
                  "38;2;223;223;0m\xE2\x96\x84\x1B[0m\n"));
}

TEST_F(LoggingTest, LogImageGrayscale) {
  std::string path =
      file::JoinPath(GetTestRootDir(), kTestDataPath, kTestImageFilename);
  MP_ASSERT_OK_AND_ASSIGN(auto image, LoadTestImage(path, ImageFormat::GRAY8));
  EXPECT_EQ(image->Format(), ImageFormat::GRAY8);

  LogImage(*image);

  EXPECT_THAT(log_lines(), HasSubstr("image[600 600 1]"));
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
%%%%%%%%%%%%%%%%%%%%%%%%%%###+::....  ........ . . .                    ... ..  ...:.:=*=+#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%*-:-..:....  ...         ..      .           .          ...:::-==**%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%#*+-+-......    .                  . .. ..        .           ..::==--=--+#%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%+*+=::.....                                        .          ...:.....:+#%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%*+*:::...     ...    .                                         .... .....:=%#%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%#*=-.:::..     .                        .....                  ........  ..:-#%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%#%#+-:...::.    ... ..         ...::::::::::------::...             ......    ..:*%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%#%#%#%#+-.-.......  .....      ..::::--==+++++++++++++++====--:::.          .   .....::=#%%%%%%%%%%%%%%%%%%%
%%%##%###%######**+==::........ ......   ..::--====++++++++++++++++++++====---:.           ...... .:-+#%%%%%%%%%%%%%%%%%
###############*=-::::..... ... .. ......::--====++++++++++++++++++++++++++====-:.            . ..-+=+%%%%%%%%%%%%%%%%%%
#############***+==:.....    ... .......::-====+++++++++++++++++++++++++++++====--:.  .   .   . ..:-+*%%%%%%%%%%%%%%%%%%
#############*#+=::.....     ... ..  .:::-===++++++++++++++++++++++++++++++=======-:.         ......:=*%%%%%%%%%%%%%%%%%
#############+++==:......   .    ..  .::-===++++++++++++*+*+***++*++++++++++++=====::..      .   . .:=-##%%%%%%%%%%%%%%%
############***+=-.... ......    ..  .:-===++++++++++++++***++*+++++++++++++++=====--::.       ..  ..::*#%%%%%%%%%%%%%%%
#############*++=:...     .  .......-====++++++++++++++++++++*+++++++++++++++++=====-:..      ....  .::=##%%%%%%%%%%%%%%
##############*+=...  .     . .:--=====+=+++++++++++++++++++++++++++++++++++++=+=====::.  .    ...  ..:=+#%%%%%%%%%%%%%%
############**+=. ..  .   ....::-=====+++++++++++++++++*++++++++++++++++++++=========--:.    ...  . ..:+*##%#%%%%%%%%%#%
#############*+:. ..     . ...:-====+=++++++++++++++++++++++*++++++++++++++++++++====-:.... ..   .. ..:+%*##%%%%%%%%%%%#
############*+:.....   .   ...:-====++++++++++++++++++++++++++++++++++++++++++++++====-:.. ...    . ...*####%%%%%%%%%###
###########**+.-...   .......:-======++++++++++++++++++++++++++++++++++++++++++++======-...           .+%###%%%%%%%%##%#
###########**=-: .. .    .. .:======++=============+=++++++++++++++++======-=====+======:              -*##%#%%%%%#%%###
##########*+++=...  .    .  .:=======--:::::.....::--==++++++====--:::...::::::----=====:.             -*##%%#%%%%####%#
############**::.  .    .   .-====---==-=====--:-::----==++++=-------::--=======-----===-.   .      ...-###%%%%%#######%
##############*=...     .. ..====--======---=--------===+++++===----==============-=====-.   .       .=##%%%%%%%########
#############**+:.    ... ..:========-:===+===-=------=+++++++==----:--===+===---========:.         ..-#%#%%%%##########
############*##*+-.   .. ..:=======---=-.:.: -.::---:--=+++++====---:::.: - ..-=---======-.        ...=%%%#%%%##########
###############**+.. ......-=====--::::-----:--==-===-=++++++=======----::.:--::::--======:  . .   ...##%%%%%%##########
#################=....... .-===++++==========---==--====+++++=======--------=============-: ...   ...:+%%%%#############
################**.........-===+===+====================++++===============------========-..... .....+##%%##############
################**:........-=+==++++++++++==============++++=========++======+=++++++====-:....:..:.+##%%%##############
###################=.......-===++=+++++++++++++++=====++++*++=============+=+++++++==+====:..:::::+##%%%##%#############
##################**- :.:..-==++===+++++++++++++=====+++++++++=========++++++++++=========:.::-=#%%%%%%%################
###################*+:::::.-=++=====++++++++++====+++++++++++=+============++++++=========:::--*#%%%%%%%%%##############
###################***-+=-:==+=========++++===========+++++++=============================-====*%%%%%%%#################
###################**#+==+=============+======-==--:.:=======-::.:--=====--======--============##%%%%%%%#####%##########
#################****#*==================-=======---------:--------=========--====----=========*%#%%%%%%%###############
###############******#===========================-------:::::--------=============-=-==========+%#%%%%%%%###############
#############**##**####+=---+=======++===================----=========================-===--=-*%%%%%%%%%##%#############
###############***********+-========++===================================================-+##%%%%#%%%%%%#####%##########
###############**********#**+===============-:..::-::--:-:-----:-:::-:-.:--====+=======--+##%%%%%%%%%%%%%#%###%#########
############**#**********#***=---======+=======--:==+**+***+#**+*+=+-::-===============-###%%%%%%%%%%%%%%%%#%###########
########**#**#***************+=---==========+++==--:---=**++*+*==-:---=============-=--*##%%%%%%%%%%%%%%%%%#############
############******************==--===========++=+=====----------=---=============-----**#%%%%%%%%%%%%%%%%%%%%%##########
#########**********************+=-=-==========+=======----======--============-------*%%%%%#%%%%%%%%%%%%%%%##%##########
#########*##*******************+==--==================------------==========--------=##%%%%%%%%%%%%%%%%%%%%%%%##########
####*######********************+==---====-===========-------:----=======----------===#%%%%%%%%%%%%%%%%%%%%%%#%##########
######**#*##**#*****************===------=--=========---------============-------=-=+%%#%%%%%%%%%%%%%%%%%%%####%%#######
#######**#**********************+===--------============================---------===-*%%%%%%%%%%%%%%%%%%%%%%%###########
#*****#**#**********************+====---------========================----------====-.=%%%%%%%%%%%%%%%%%%#%%###%########
#####***************************+==-===-----------==============-===-----------=====: :-+%#%%%#%%%%%%%%%%%%%%%%#########
******************************+:-===-=---------:------------------------------======. .:---*%#%%%#%%%%%%%%###%###%######
###**************************-:.:=====-----------:::::---::--:::::::::-------=======. .:-:------=#%#%#%%%%%%%%##########
**************************=::::. =====----------:::::::....:...:::::::-----=========- .:--:::::----=#%%###%%##%%########
***********************=::::::.. ========--------::::::::::.::::::--------==========-.::::::::::--::--##%%##%###########
********************+::.:::.... :========-------------------------------============:.::-::::::--:::::-=#%%%############
***************-::::.:.:.:..... :-=============-===============-====--==============::-::::::::::::::::----*%###########
***********+--------......:.... :==================================================:.::::::::::::::::::-----=###%#######
********+--------:---.....:..... =================================================::::::::::::::::::-::---------#%######
******=--::::-:::--:::   .:..... -=============================================-:::::::::::::::::--::::-----:-:----=+###
****=:::::::::::::::::-:. .:..... -=============================---::::::::::.::..:::::::::::::::::::::-::::::::::::----)"));
}

TEST_F(LoggingTest, LogImageRGB) {
  std::string path =
      file::JoinPath(GetTestRootDir(), kTestDataPath, kTestImageFilename);
  MP_ASSERT_OK_AND_ASSIGN(auto image, LoadTestImage(path, ImageFormat::SRGB));
  EXPECT_EQ(image->Format(), ImageFormat::SRGB);

  LogImage(*image);
  EXPECT_THAT(log_lines(), HasSubstr("image[600 600 3]"));
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
%%%%%%%%%%%%%%%%%%%%%%%%%%###+::....  ........ . . .                    ... .. . ..:.:=*=+#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%*-:-..:....  ..          ..      .           .          ...:::-==**%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%#*+-+-......    .                  . .. ..        .           ..::-=--=--+#%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%+*==::.. ..                                        .          ...:.....:+#%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%*+*:::...     ...    .                                         .... .....:=%#%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%#*=-.:::.      .                        .....                  ........  ..:-#%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%#%#+-:...::.    ... ..         ...::::::::::------::...             ......    ..:*%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%#%#%#+-.:.......  .....      ..::::--==+++++++++++++++====--:::.          .   .....::=#%%%%%%%%%%%%%%%%%%%
%%%######%######**+==::........ ......   ..::--====++++++++++++++++++++=====--:.           ...... .:-+#%%%%%%%%%%%%%%%%%
##############**=-:::...... ... .. .. ...::--====++++++++++++++++++++++++++====-:.            . ..-+=+%%%%%%%%%%%%%%%%%%
#############***+==:....     ... .......::-====+++++++++++++++++++++++++++++====--:.  .   .   . ..:-+*%%%%%%%%%%%%%%%%%%
#############*#+=::.....     ... ..   :::-===++++++++++++++++++++++++++++++=======-:.         ......:=*%%%%%%%%%%%%%%%%%
#############+++==:......   .    ..  .::-===++++++++++++++*+++++++++++++++++++=====::..      .   . ..=-##%%%%%%%%%%%%%%%
############***+=-.... ......    ..  .:-===+++++++++++++++**++*+++++++++++++++=====--::        ..  ..::*#%%%%%%%%%%%%%%%
#############*++=:...        .......-====++++++++++++++++++++*++++++++++++++++======-:..      ....  .::=##%%%%%%%%%%%%%%
##############*+=...  .     . .:--=======+++++++++++++++++++++++++++++++++++++=======::.       ..   ..:=+#%%%%%%%%%%%%%%
############**+=. ..       ...::-======++++++++++++++++*++++++++++++++++++++=========--..    ...  . ..:+*##%%%%%%%%%%%%%
#############*+:. ..     . ...:-====+=++++++++++++++++++++++*+++++++++++++++++=+=====-:.... ..   .. ..:+#*##%%%%%%%%%%%#
############*+:.....   .   ...:-=====++++++++++++++++++++++++++++++++++++++++++++=====-:.. . .      ...*####%%%%%%%%%%##
###########**+.-  .   .......:-======++++++++++++++++++++++++++++++++++++++++++=======--...           .+%###%%%%%%%%#%%#
###########**=-: .. .    .. .:======++=============+++++++++++++++++++=====-=====+======:              -*##%%%%%%%%%%###
###########+++=..        .  .:=======--:::::.....::--==++++++====--:::...::::::----=====:              -*##%%#%%%%####%#
############**::.  .    .   .-====---==-=====-::--:----===+++=-------::--=======-----===-.   .      ...-###%%%%%#######%
##############+-...     .. ..====--======---=--------===+++++===----==============-=====-.   .       .-##%%%%%%%#######%
#############**+:.    ... ..:========-:===+===-=------==++++++==----:--===+===---========:.         ..-#%%%%%%%#########
############*##*+-    .. ..:=======-:-=-...: -.::---:--=+++++==-=---:::.: - ..-=----=====-.        ...=%%%#%%%%#######%#
###############**+.. ..... -=====--:::::----:--==-==--==+++++=======-----:.:--::::---=====:  .     ...##%%%%%%%#########
################*=....... .-====++===========---=---====+++++=======--------=============-: ...   ...:+%%%%%############
################** ........-===+===+====================++++===============------========-..... .....+##%%%###%#########
################**:........-====++++++++++==============++++++=======++======+=++++++====-:....:..:.+##%%%%#############
###################=.......-===++=+++++++++++++++=====++++*++============++=+++++++==+====:..:::::+##%%%%%%##%##########
##################**- :.:..-==++==++++++++++++++=====+++++++++=========+++++++++++========:.::-=#%%%%%%%%#%#############
###################*+:::::.-=++=====++++++++++====++++++++++++++=++========+++++++========::---*#%%%%%%%%%##############
###################***-+=-:===========+++++===========+++++++=============================-====*%%%%%%%%#%%%############
####################**+==+=============+======-==--:.:=======-::.:--=====--======--============##%%%%%%%##%%#%##########
###################**#*==================-=======---------:--------=========--====----=========*%#%%%%%%%#%#############
####################*#===========================-------:::::-------==============-=-==========+%#%%%%%%%#%#############
#################*#####+=---=============================----=======================-=-===--=-#%%%%%%%%%%%%%############
########*#####*#**********+-========++===============================================--==-+##%#%%%%%%%%%##%%#%##########
###########****#*********#**+===============-:..::-:-----:-----:--::-:-.:--============--+##%%%%%%%%%%%%%%%%##%#########
############**#####******#***=---==============--:+++**+***+#**+*+=+-::-===============-###%%%%%%%%%%%%%%%%%%###########
###########*####*************+=---==========++++=-=:---=*+++*+*==-:---=============----+##%%%%%%%%%%%%%%%%%%%%##########
###############***************==----=========++=+======--------===-============-------**#%%%%%%%%%%%%%%%%%%%%%##########
##########*********************+=-=-==========+=======---========-============-------*%#%%%#%%%%%%%%%%%%%%%%%%##########
#########*##*******************+==--=====-=============----------======-====--------=##%%%%%%%%%%%%%%%%%%%%%%%%%%%######
####*#######*******************+==----===-===========------------=====-=----------===#%%%%%%%%%%%%%%%%%%%%%%%%%#########
######**#*##**#*****************===------=---========---------===========----------=+%%%%%%%%%%%%%%%%%%%%%%%%%#%%#######
******#**#**********************====---------===========================---------===-*%%%%%%%%%%%%%%%%%%%%%%%%%%########
#*##**#**#**********************+====---------========+===============----------====-.=%%%%%%%%%%%%%%%%%%%%%%%%%########
#####***************************===--==-----------==============-===-----------=====: :-+%#%#%#%%%%%%%%%%%%%%%%%%%######
*#****************************+:-===-----------:------=--------------:--------======. .:---*%#%#%#%%%%%%%%%%%%%#%%%###%#
###**************************-:.:=====----------::::::-------:::::::::--------======. .:--------=#%#%#%%%%%%%%%#########
**************************=::::. =====---------::::::::....:...:::::::------========- .:--::::-----=#%%##%%%##%%########
***********************=::::::.. =======---------::::::.:::.::::::--------==========-.::-:::::::--::--##%%%#%###########
********************+::::::::.. :========----------------:--------------============:.::-:::::---:::::-=##%%############
***************-::::.:::.::.... :-=========-===-=-===-====-====-====---============-::-:::::::-::::::-:----*%###########
***********+--------.....::.... :==================================================:.:::::::::::::::::------=###%#######
********+------------.....:..... =================================================::::::::::::::::::-::---------#%######
******=---:::-:::--::: . .:..... :-============================================-:::::::::::::::::--:-::-----:-:----=+###
****=:::::::::::::::-:-:. .:..... --============================---::::::::::.::..:::::::::::::::::::::-::::::-:-:::----)"));
}

TEST_F(LoggingTest, LogImageRGBAColor) {
  std::string path =
      file::JoinPath(GetTestRootDir(), kTestDataPath, kTestImageFilename);
  MP_ASSERT_OK_AND_ASSIGN(auto image, LoadTestImage(path, ImageFormat::SRGBA));
  EXPECT_EQ(image->Format(), ImageFormat::SRGBA);

  // Add some transparency (circle with soft fade-out).
  float R = image->Width() * 0.45f;
  float dR = R * 0.1f;
  int cx = image->Width() / 2;
  int cy = image->Height() / 2;
  for (int y = 0; y < image->Height(); ++y) {
    for (int x = 0; x < image->Width(); ++x) {
      float r = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
      float alpha = std::clamp((R - r) / dR, 0.0f, 1.0f);
      image->MutablePixelData()[y * image->WidthStep() + x * 4 + 3] =
          static_cast<uint8_t>(alpha * 255.0f);
    }
  }

  // CiderV's terminal actually supports true color, so that the image shows up
  // correctly. Unfortunately, the editor doesn't.
  setenv(kColorTerm, "truecolor", 1);
  LogImage(*image);
  EXPECT_THAT(log_lines(),
              HasSubstr("\xE2\x95\x94\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90"));
}

TEST_F(LoggingTest, LogMat) {
  cv::Mat mat = cv::Mat::zeros(10, 10, CV_32FC2);
  for (int i = 0; i < mat.rows; ++i) {
    for (int j = 0; j < mat.cols; ++j) {
      mat.at<cv::Vec2f>(i, j)[0] = (i + 0.5f) / 10.0f;
      mat.at<cv::Vec2f>(i, j)[1] = (j + 0.5f) / 10.0f;
    }
  }
  LogMat(mat);

  EXPECT_THAT(log_lines(), HasSubstr("mat[10 10 2]"));
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
 ..::--==+
.::--==++*
:--==++**#
-==++**##%
=++**##%%@)"));
}

TEST_F(LoggingTest, LogHalideBufferGrayscale) {
  Halide::Runtime::Buffer<uint8_t> buffer(10, 10);
  buffer.for_each_element([&](int x, int y) {
    buffer(x, y) = static_cast<uint8_t>((x + 0.5f + y + 0.5f) * 255 / 20);
  });
  LogHalideBuffer(buffer);

  EXPECT_THAT(log_lines(), HasSubstr("buffer[10 10]"));
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
 ..::--==+
.::--==++*
:--==++**#
-==++**##%
=++**##%%@)"));
}

TEST_F(LoggingTest, LogHalideBufferRGBInterleaved) {
  auto buffer = Halide::Runtime::Buffer<uint8_t>::make_interleaved(10, 10, 3);
  buffer.for_each_element([&](int x, int y) {
    buffer(x, y, 0) = x < 5 ? 255 : 0;
    buffer(x, y, 1) = y < 5 ? 255 : 0;
    buffer(x, y, 2) = x < 5 && y < 5 ? 255 : 0;
  });
  LogHalideBuffer(buffer);

  EXPECT_THAT(log_lines(), HasSubstr("buffer[10 10 3]"));
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
@@@@@-----
@@@@@-----
*****.....
-----
-----     )"));
}

TEST_F(LoggingTest, LogHalideBufferRGBPlanar) {
  auto buffer = Halide::Runtime::Buffer<uint8_t>(10, 10, 3);
  buffer.for_each_element([&](int x, int y) {
    buffer(x, y, 0) = x < 5 ? 255 : 0;
    buffer(x, y, 1) = y < 5 ? 255 : 0;
    buffer(x, y, 2) = x < 5 && y < 5 ? 255 : 0;
  });
  LogHalideBuffer(buffer);

  EXPECT_THAT(log_lines(), HasSubstr("buffer[10 10 3]"));
  EXPECT_THAT(log_lines(), HasConsecutiveLines(R"(
@@@@@-----
@@@@@-----
*****.....
-----
-----     )"));
}

TEST_F(LoggingTest, LogHalideBufferOneDimensional) {
  auto buffer = Halide::Runtime::Buffer<uint8_t>(10);
  buffer.for_each_element(
      [&](int x) { buffer(x) = static_cast<uint8_t>((x + 0.5f) * 255 / 10); });
  LogHalideBuffer(buffer);

  EXPECT_THAT(log_lines(), HasSubstr("buffer[10]"));
  EXPECT_THAT(log_lines(), HasSubstr(" .:-=+*#%@"));
}

TEST_F(LoggingTest, LogHalideBufferFourDimensional) {
  EXPECT_CALL(log(),
              Log(absl::LogSeverity::kWarning, _, HasSubstr("cannot log")));
  auto buffer = Halide::Runtime::Buffer<uint8_t>(1, 2, 3, 4);
  buffer.for_each_element(
      [&](int x, int y, int z, int w) { buffer(x, y, z, w) = 0; });
  LogHalideBuffer(buffer);
}

TEST_F(LoggingTest, LogHalideBufferEmpty) {
  Halide::Runtime::Buffer<uint8_t> buffer;
  LogHalideBuffer(buffer);

  EXPECT_THAT(log_lines(), HasSubstr("buffer[]"));
  EXPECT_THAT(log_lines(), HasSubstr("<empty>"));
}

}  // namespace
}  // namespace mediapipe::debug
