/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "camera_capture.h"
#include <jpeglib.h>
int minmax(int min, int v, int max)
{
  return (v < min) ? min : (max < v) ? max : v;
}

int init_capture(void *arg)
{
	struct v4l2_input inp;
	struct v4l2_streamparm parms;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	int n_buffers;

	hawkview_handle *hv = (hawkview_handle*)arg;
	hv->capture.cap_fps = 30;
	hv->capture.buf_count = 10;

	/* set capture input */
	inp.index = 0;
	inp.type = V4L2_INPUT_TYPE_CAMERA;
	if (ioctl(hv->detect.videofd, VIDIOC_S_INPUT, &inp) == -1) {
		hv_err("VIDIOC_S_INPUT failed! s_input: %d\n",inp.index);
		return -1;
	}

	/* set capture parms */
	parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parms.parm.capture.timeperframe.numerator = 1;
	parms.parm.capture.timeperframe.denominator = hv->capture.cap_fps;
	if (ioctl(hv->detect.videofd, VIDIOC_S_PARM, &parms) == -1) {
		hv_err("VIDIOC_S_PARM failed!\n");
		return -1;
	}

	/* get and print capture parms */
	memset(&parms, 0, sizeof(struct v4l2_streamparm));
	parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(hv->detect.videofd, VIDIOC_G_PARM, &parms) == 0) {
		hv_msg(" Camera capture framerate is %u/%u\n",
				parms.parm.capture.timeperframe.denominator, \
				parms.parm.capture.timeperframe.numerator);
	} else {
		hv_err("VIDIOC_G_PARM failed!\n");
	}

	/* set image format */
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width			= hv->capture.cap_w;
	fmt.fmt.pix.height			= hv->capture.cap_h;
	fmt.fmt.pix.pixelformat		= hv->capture.cap_fmt;
	fmt.fmt.pix.field			= V4L2_FIELD_NONE;
	if (ioctl(hv->detect.videofd, VIDIOC_S_FMT, &fmt) < 0) {
		hv_err("VIDIOC_S_FMT failed!\n");
		return -1;
	}

	/* reqbufs */
	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count  = hv->capture.buf_count;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(hv->detect.videofd, VIDIOC_REQBUFS, &req) < 0) {
		hv_err("VIDIOC_REQBUFS failed\n");
		return -1;
	}

	/* mmap hv->capture.buffers */
	hv->capture.buffers = calloc(req.count, sizeof(struct buffer));
	for (n_buffers= 0; n_buffers < req.count; ++n_buffers) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= n_buffers;

		if (ioctl(hv->detect.videofd, VIDIOC_QUERYBUF, &buf) == -1) {
			hv_err("VIDIOC_QUERYBUF error\n");
			return -1;
		}

		hv->capture.buffers[n_buffers].length= buf.length;
		hv->capture.buffers[n_buffers].start = mmap(NULL , buf.length,
										PROT_READ | PROT_WRITE, \
										MAP_SHARED , hv->detect.videofd, \
										buf.m.offset);
		hv_msg("map buffer index: %d, mem: 0x%x, len: %x, offset: %x\n",
				n_buffers,(int)hv->capture.buffers[n_buffers].start,buf.length,buf.m.offset);
	}

	/* qbuf */
	for(int n_buffers = 0; n_buffers < req.count; n_buffers++) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= n_buffers;
		if (ioctl(hv->detect.videofd, VIDIOC_QBUF, &buf) == -1) {
			hv_err("VIDIOC_QBUF error\n");
			return -1;
		}
	}

	/* create fifo */
	hv->capture.fifo = FIFO_Creat(req.count*2, sizeof(struct v4l2_buffer));
}

int capture_quit(void *arg)
{
	int i;
	enum v4l2_buf_type type;
	hawkview_handle *hv = (hawkview_handle*)arg;

	hv_msg("capture quit!\n");

	/* streamoff */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(hv->detect.videofd, VIDIOC_STREAMOFF, &type) == -1) {
		hv_err("VIDIOC_STREAMOFF error! %s\n",strerror(errno));
		return -1;
	}

	/* munmap hv->capture.buffers */
	for (i = 0; i < hv->capture.buf_count; i++) {
		hv_dbg("ummap index: %d, mem: %x, len: %x\n",
				i,(unsigned int)hv->capture.buffers[i].start,(unsigned int)hv->capture.buffers[i].length);
		munmap(hv->capture.buffers[i].start, hv->capture.buffers[i].length);
	}

	/* free hv->capture.buffers and close hv->detect.videofd */
	free(hv->capture.buffers);
	close(hv->detect.videofd);

	return 0;
}

uint8_t* yuyv2rgb(uint8_t* yuyv, uint32_t width, uint32_t height)
{
  FILE *fp;
  int iim;
  uint8_t* rgb = (uint8_t*)calloc(width * height * 3, sizeof (uint8_t));
  printf("yuyv2rgb\n");
  for (size_t i = 0; i < height; i++) {
    for (size_t j = 0; j < width; j += 2) {
      size_t index = i * width + j;
      int y0 = yuyv[index * 2 + 0] << 8;
      int u = 0x00;
      int y1 = yuyv[index * 2 + 2] << 8;
      int v = 0x00;
      rgb[index * 3 + 0] = minmax(0, (y0 + 359 * v) >> 8, 255);
      rgb[index * 3 + 1] = minmax(0, (y0 + 88 * v - 183 * u) >> 8, 255);
      rgb[index * 3 + 2] = minmax(0, (y0 + 454 * u) >> 8, 255);
      rgb[index * 3 + 3] = minmax(0, (y1 + 359 * v) >> 8, 255);
      rgb[index * 3 + 4] = minmax(0, (y1 + 88 * v - 183 * u) >> 8, 255);
      rgb[index * 3 + 5] = minmax(0, (y1 + 454 * u) >> 8, 255);
    }
  }
  return rgb;
}
void jpeg(FILE* dest, uint8_t* rgb, uint32_t width, uint32_t height, int quality)
{
  JSAMPARRAY image;
  image = (JSAMPARRAY)calloc(height, sizeof (JSAMPROW));
  for (size_t i = 0; i < height; i++) {
    image[i] = (uint8_t*)calloc(width * 3, sizeof (JSAMPLE));
    for (size_t j = 0; j < width; j++) {
      image[i][j * 3 + 0] = rgb[(i * width + j) * 3 + 0];
      image[i][j * 3 + 1] = rgb[(i * width + j) * 3 + 1];
      image[i][j * 3 + 2] = rgb[(i * width + j) * 3 + 2];
    }
  }
  struct jpeg_compress_struct compress;
  struct jpeg_error_mgr error;
  compress.err = jpeg_std_error(&error);
  jpeg_create_compress(&compress);
  jpeg_stdio_dest(&compress, dest);
  compress.image_width = width;
  compress.image_height = height;
  compress.input_components = 3;
  compress.in_color_space = JCS_RGB;
  jpeg_set_defaults(&compress);
  jpeg_set_quality(&compress, quality, TRUE);
  jpeg_start_compress(&compress, TRUE);
  jpeg_write_scanlines(&compress, image, height);
  jpeg_finish_compress(&compress);
  jpeg_destroy_compress(&compress);
  for (size_t i = 0; i < height; i++) {
    free(image[i]);
  }
  free(image);
}
void *Capture_Test_Thread(void *arg)
{
	enum v4l2_buf_type type;
	struct timeval tv;
	struct timeval timestart;
	fd_set fds;
	struct v4l2_buffer buf;
	int ret;
	int np = 0;
	long long timestamp_now, timestamp_save;
	char source_data_path[30];
	char out_name[64];
        unsigned char* rgb;

	hawkview_handle *hv = (hawkview_handle*)arg;

	/* streamon */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(hv->detect.videofd, VIDIOC_STREAMON, &type) == -1) {
		hv_err("VIDIOC_STREAMON error! %s\n",strerror(errno));
		return (void*)0;
	}

	FD_ZERO(&fds);
	FD_SET(hv->detect.videofd, &fds);

	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
        gettimeofday(&timestart, NULL);
        printf("capture start no save ==> %ld:%ld \n",timestart.tv_sec, timestart.tv_usec);

	while (np < hv->detect.frame_num) {
		hv_msg("capture num is [%d]\n",np);
		/* wait for sensor capture data */
		tv.tv_sec  = 1;
		tv.tv_usec = 0;
		ret = select(hv->detect.videofd + 1, &fds, NULL, NULL, &tv);
		if (ret == -1)
			hv_err("select error\n");
		else if (ret == 0)
			hv_err("select timeout\n");

		/* dqbuf */
		ret = ioctl(hv->detect.videofd, VIDIOC_DQBUF, &buf);
		if (ret == 0)
			hv_msg("\n*****DQBUF[%d] FINISH*****\n",buf.index);
		else
			hv_err("****DQBUF FAIL*****\n");

		timestamp_now = secs_to_msecs((long long)(buf.timestamp.tv_sec), (long long)(buf.timestamp.tv_usec));
		if (np == 0) {
			timestamp_save = timestamp_now;
		}
		hv_msg("the interval of two frames is %lld ms\n", timestamp_now - timestamp_save);
		timestamp_save = timestamp_now;

		if (hv->capture.cap_fmt == V4L2_PIX_FMT_NV21) {
		    sprintf(source_data_path, "%s/%s%d%s", hv->detect.path, "source_data", np+1, ".yuv");
        save_frame_to_file(source_data_path, hv->capture.buffers[buf.index].start, buf.bytesused);

		}else if( hv->capture.cap_fmt == V4L2_PIX_FMT_YUYV) {
		    rgb = yuyv2rgb(hv->capture.buffers[buf.index].start, hv->capture.cap_w, hv->capture.cap_h);
		}
		/* qbuf */
		if (ioctl(hv->detect.videofd, VIDIOC_QBUF, &buf) == 0)
			hv_msg("************QBUF[%d] FINISH**************\n",buf.index);

		if( hv->capture.cap_fmt == V4L2_PIX_FMT_YUYV) {
		    sprintf(out_name,"%s/%011lld.jpg",hv->detect.path,timestamp_now);
		    FILE* out = fopen(out_name, "w");
		    jpeg(out, rgb, hv->capture.cap_w, hv->capture.cap_h, 100);
		    fclose(out);
		}
		np++;
	}
	gettimeofday(&timestart, NULL);
	hv_msg("capture thread finish %ld:%ld !\n", timestart.tv_sec, timestart.tv_usec);
	//capture_quit(hv);
	return (void*)0;
}
