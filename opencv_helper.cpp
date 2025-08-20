#include <opencv2/opencv.hpp>
#include <stdint.h>

using namespace cv;

extern "C" {

void convert_bayer_to_rgb(char* bayer,
                                     int width, int height,
                                     int bpp, char* output) {
	int type_code = 0;
	uint32_t mask  = 0x00000000;

	switch (bpp) {
	case 10:
		mask  = 0x000003FF;
		type_code = CV_16UC1; // 16-bit align - bayer
		break;
	case 12:
		mask = 0x00000FFF;
		type_code = CV_16UC1; // 16-bit align - bayer
		break;
	case 14:
		mask = 0x00003FFF;
		type_code = CV_16UC1; // 16-bit align - bayer
		break;
	case 16:
		mask = 0x0000FFFF;
		type_code = CV_16UC1; // 16-bit align - bayer
		break;
	case 20:
		mask = 0x000FFFFF;
		type_code = CV_32SC1; // 32-bit align - bayer
		break;
	case 24:
		mask = 0x00FFFFFF;
		type_code = CV_32SC1; // 32-bit align - bayer
		break;
	case 28:
		mask = 0x0FFFFFFF;
		type_code = CV_32SC1; // 32-bit align - bayer
		break;
	default:
		fprintf(stderr, "Unsupported bit-depth %d\n", bpp);
		return;
	}

	// Wrap input into cv::Mat
    cv::Mat bayerMat(height, width, type_code, bayer);

	bayerMat &= mask;

	// Select Bayer pattern
    int code = cv::COLOR_BayerRGGB2BGR;

    // Convert to type_code's bit-depth BGR
    cv::Mat bgr;
    cv::cvtColor(bayerMat, bgr, code);

    // Normalize down to 8-bit for display
	double scale_factor = (1 << bpp) / 256.0;
    cv::Mat bgr8;
    bgr.convertTo(bgr8, CV_8UC3, 1.0 / scale_factor);

	// Convert to BGRA (adds alpha channel)
	cv::Mat bgra;
	cv::cvtColor(bgr8, bgra, cv::COLOR_BGR2BGRA);

   // Fill alpha/X channel with 255
   for (int y = 0; y < height; y++) {
       cv::Vec4b* row = bgra.ptr<cv::Vec4b>(y);
       for (int x = 0; x < width; x++) {
           row[x][3] = 255; // set A=255 or X=0
       }
   }

    // Copy result to output buffer
    memcpy(output, bgra.data, width * height * 4);

	return;
}

} // extern "C"
