#include "segment/image.h"
#include "segment/misc.h"
#include "segment/filter.h"
#include "segment/segment-graph.h"
#include "segment.h"

// random color
rgb random_rgb(){ 
  rgb c;
  double r;
  
  c.r = (uchar)random();
  c.g = (uchar)random();
  c.b = (uchar)random();

  return c;
}


// dissimilarity measure between pixels
static inline float diff(image<float> *r, image<float> *g, image<float> *b,
			 int x1, int y1, int x2, int y2) {
  return sqrt(square(imRef(r, x1, y1)-imRef(r, x2, y2)) +
	      square(imRef(g, x1, y1)-imRef(g, x2, y2)) +
	      square(imRef(b, x1, y1)-imRef(b, x2, y2)));
}

/*
 * Segment an image
 *
 * Returns index of the components (rather than the random colors) representing the segmentation.
 *
 * im: image to segment.
 * sigma: to smooth the image.
 * c: constant for treshold function.
 * min_size: minimum component size (enforced by post-processing stage).
 * num_ccs: number of connected components in the segmentation.
 */
image<int> *segment_image(image<rgb> *im, float sigma, float c, int min_size,
			  int *num_ccs) {
  int width = im->width();
  int height = im->height();

  image<float> *r = new image<float>(width, height);
  image<float> *g = new image<float>(width, height);
  image<float> *b = new image<float>(width, height);

  // smooth each color channel  
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      imRef(r, x, y) = imRef(im, x, y).r;
      imRef(g, x, y) = imRef(im, x, y).g;
      imRef(b, x, y) = imRef(im, x, y).b;
    }
  }
  image<float> *smooth_r = smooth(r, sigma);
  image<float> *smooth_g = smooth(g, sigma);
  image<float> *smooth_b = smooth(b, sigma);
  delete r;
  delete g;
  delete b;
 
  // build graph
  edge *edges = new edge[width*height*4];
  int num = 0;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if (x < width-1) {
	edges[num].a = y * width + x;
	edges[num].b = y * width + (x+1);
	edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x+1, y);
	num++;
      }

      if (y < height-1) {
	edges[num].a = y * width + x;
	edges[num].b = (y+1) * width + x;
	edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x, y+1);
	num++;
      }

      if ((x < width-1) && (y < height-1)) {
	edges[num].a = y * width + x;
	edges[num].b = (y+1) * width + (x+1);
	edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x+1, y+1);
	num++;
      }

      if ((x < width-1) && (y > 0)) {
	edges[num].a = y * width + x;
	edges[num].b = (y-1) * width + (x+1);
	edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x+1, y-1);
	num++;
      }
    }
  }
  delete smooth_r;
  delete smooth_g;
  delete smooth_b;

  // segment
  universe *u = segment_graph(width*height, num, edges, c);
  
  // post process small components
  for (int i = 0; i < num; i++) {
    int a = u->find(edges[i].a);
    int b = u->find(edges[i].b);
    if ((a != b) && ((u->size(a) < min_size) || (u->size(b) < min_size)))
      u->join(a, b);
  }
  delete [] edges;
  *num_ccs = u->num_sets();

  // set comp index
  image<int> *output = new image<int>(width, height);
  int *colors = new int[width * height];
  for (int i = 0; i < width*height; i++)
      colors[i] = 0;

  int curr_idx = 1;
  for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
          int comp = u->find(y * width + x);
          if (!colors[comp]) {
              colors[comp] = curr_idx;
              curr_idx++;
          }
          imRef(output, x, y) = colors[comp];
      }
  }
  delete [] colors;
  delete u;

  return output;
}


/*
 * OpenCV wrapper
 *
 * input:   CV_8UC3
 * output:  CV_32SC1
 * return:  num_ccs
 */
int segment(const cv::Mat &input, cv::Mat &output, float sigma, float c, int min_size) {
    int height = input.rows;
    int width  = input.cols;

    // copy input image
    image<rgb> *input_rgb = new image<rgb>(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            rgb color;
            cv::Vec3b color_ = input.at<cv::Vec3b>(y, x);
            color.b = color_.val[0];
            color.g = color_.val[1];
            color.r = color_.val[2];
            imRef(input_rgb, x, y) = color;
        }
    }

    // segment
    image<int> *output_comp;
    int num_ccs;
    output_comp = segment_image(input_rgb, sigma, c, min_size, &num_ccs);

    // copy results
    output.create(height, width, CV_32SC1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            output.at<int>(y, x) = imRef(output_comp, x, y) - 1;
        }
    }
    delete input_rgb;
    delete output_comp;

    return num_ccs;
}


// draw segment with random colors
void draw_segment(const cv::Mat &input_comp, cv::Mat &output) {
    int height = input_comp.rows;
    int width  = input_comp.cols;

    if (output.empty())
        output.create(height, width, CV_8UC3);

    rgb *colors = new rgb[width * height];
    for (int i = 0; i < width * height; ++i)
        colors[i] = random_rgb();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            rgb c = colors[input_comp.at<int>(y, x)];
            output.at<cv::Vec3b>(y, x).val[0] = c.b;
            output.at<cv::Vec3b>(y, x).val[1] = c.g;
            output.at<cv::Vec3b>(y, x).val[2] = c.r;
        }
    }
}
