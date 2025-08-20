#ifdef __cplusplus
extern "C" {
#endif

void convert_bayer_to_rgb(char* bayer,
                          int width, int height,
                          int bpp, char* output);

#ifdef __cplusplus
}
#endif

