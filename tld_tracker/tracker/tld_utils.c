#include "tld_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define PROPOSAL_SOURCE_TRACKER 0
#define PROPOSAL_SOURCE_DETECTOR 1
#define PROPOSAL_SOURCE_MIXED 2
#define PROPOSAL_SOURCE_FINAL 3

void print_rect(FILE* out, const Rect* rect) {
    fprintf(out, "Rect: %d %d %d %d\n", rect->x, rect->y, rect->width, rect->height);
}

int candidate_less(const Candidate* a, const Candidate* b) {
    return a->prob < b->prob;
}

double get_normalized_random() {
    return (double)rand() / (double)RAND_MAX;
}

int get_random_int(int maxint) {
    if (maxint <= 0) return 0;
    return rand() % (maxint + 1);
}

Image generate_random_image() {
    int width = 640, height = 480;
    Image out;
    out.width = width;
    out.height = height;
    out.data = (uint8_t*)malloc(width * height);
    if (!out.data) { out.width = 0; out.height = 0; }
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++)
            out.data[j * width + i] = (uint8_t)get_random_int(255);
    return out;
}

Image generate_random_image_with_size(int width, int height) {
    Image out;
    out.width = width;
    out.height = height;
    out.data = (uint8_t*)malloc(width * height);
    if (!out.data) { out.width = 0; out.height = 0; }
    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++)
            out.data[j * width + i] = (uint8_t)get_random_int(255);
    return out;
}

void image_free(Image* img) {
    if (img && img->data) free(img->data);
    img->data = NULL;
}

Rect get_extended_rect_for_rotation(Rect base_rect, double angle_degrees) {
    double center_x = base_rect.x + 0.5 * base_rect.width;
    double center_y = base_rect.y + 0.5 * base_rect.height;
    double angle_rad = fabs(angle_degrees * M_PI / 180.0);
    int size_x = (int)(base_rect.width * (cos(angle_rad) + sin(angle_rad)));
    int size_y = (int)(base_rect.height * (cos(angle_rad) + sin(angle_rad)));
    int x = (int)(center_x - 0.5 * size_x);
    int y = (int)(center_y - 0.5 * size_y);
    Rect out = { x, y, size_x, size_y };
    return out;
}

Image image_create(int width, int height) {
    Image img;
    img.width = width;
    img.height = height;
    img.data = (uint8_t*)malloc(width * height);
    if (!img.data) { img.width = 0; img.height = 0; }
    return img;
}

Image image_crop(const Image* src, Rect roi) {
    Image out = *src;
    out.data = src->data + roi.y * src->width + roi.x;
    out.width = roi.width;
    out.height = roi.height;
    return out;
}

void image_copy_region(const Image* src, Image* dst, int x0, int y0) {
    for (int j = 0; j < src->height; j++) {
        for (int i = 0; i < src->width; i++) {
            int di = (y0 + j) * dst->width + (x0 + i);
            int si = j * src->width + i;
            dst->data[di] = src->data[si];
        }
    }
}

Size image_size(const Image* img) {
    Size sz;
    sz.width = img->width;
    sz.height = img->height;
    return sz;
}

// --- MISSING image_subframe_clone ---
Image image_subframe_clone(const Image* src, Rect roi) {
    Image sub;
    sub.width = roi.width;
    sub.height = roi.height;
    sub.data = malloc(sub.width * sub.height);
    for (int y = 0; y < roi.height; ++y) {
        for (int x = 0; x < roi.width; ++x) {
            int src_x = roi.x + x;
            int src_y = roi.y + y;
            if (src_x >= 0 && src_x < src->width && src_y >= 0 && src_y < src->height) {
                sub.data[y * sub.width + x] = src->data[src_y * src->width + src_x];
            } else {
                sub.data[y * sub.width + x] = 0;
            }
        }
    }
    return sub;
}
void image_rotate(const Image* src, Image* dst, double angle_deg, double cx, double cy) {
    double angle_rad = angle_deg * M_PI / 180.0;
    double cos_a = cos(angle_rad);
    double sin_a = sin(angle_rad);
    for (int y = 0; y < dst->height; y++) {
        for (int x = 0; x < dst->width; x++) {
            double tx = x - cx;
            double ty = y - cy;
            double sx = cos_a * tx + sin_a * ty + cx;
            double sy = -sin_a * tx + cos_a * ty + cy;
            int src_x = (int)round(sx);
            int src_y = (int)round(sy);
            if (src_x >= 0 && src_x < src->width && src_y >= 0 && src_y < src->height)
                dst->data[y * dst->width + x] = src->data[src_y * src->width + src_x];
            else
                dst->data[y * dst->width + x] = 0;
        }
    }
}

Image get_rotated_subframe(const Image* frame, Rect subframe_rect, double angle_deg) {
    Rect ext = get_extended_rect_for_rotation(subframe_rect, angle_deg);
    Image src_subframe = image_create(ext.width, ext.height);
    for (int j = 0; j < ext.height; j++)
        for (int i = 0; i < ext.width; i++) {
            int fx = ext.x + i;
            int fy = ext.y + j;
            src_subframe.data[j * ext.width + i] =
                (fx >= 0 && fx < frame->width && fy >= 0 && fy < frame->height)
                ? frame->data[fy * frame->width + fx]
                : 0;
        }
    Image rotated = image_create(ext.width, ext.height);
    image_rotate(&src_subframe, &rotated, angle_deg, ext.width/2.0, ext.height/2.0);
    int offset_x = (int)(0.5 * (ext.width - subframe_rect.width));
    int offset_y = (int)(0.5 * (ext.height - subframe_rect.height));
    Image result = image_create(subframe_rect.width, subframe_rect.height);
    for (int j = 0; j < subframe_rect.height; j++)
        for (int i = 0; i < subframe_rect.width; i++)
            result.data[j * subframe_rect.width + i] =
                rotated.data[(offset_y + j) * ext.width + (offset_x + i)];
    image_free(&src_subframe);
    image_free(&rotated);
    return result;
}

void rotate_subframe(Image* frame, Rect subframe_rect, double angle_deg) {
    Rect ext = get_extended_rect_for_rotation(subframe_rect, angle_deg);
    Image src_subframe = image_create(ext.width, ext.height);
    for (int j = 0; j < ext.height; j++)
        for (int i = 0; i < ext.width; i++) {
            int fx = ext.x + i;
            int fy = ext.y + j;
            src_subframe.data[j * ext.width + i] =
                (fx >= 0 && fx < frame->width && fy >= 0 && fy < frame->height)
                ? frame->data[fy * frame->width + fx]
                : 0;
        }
    Image rotated = image_create(ext.width, ext.height);
    image_rotate(&src_subframe, &rotated, angle_deg, ext.width/2.0, ext.height/2.0);
    int offset_x = (int)(0.5 * (ext.width - subframe_rect.width));
    int offset_y = (int)(0.5 * (ext.height - subframe_rect.height));
    for (int j = 0; j < subframe_rect.height; j++) {
        int fy = subframe_rect.y + j;
        if (fy < 0 || fy >= frame->height) continue;
        for (int i = 0; i < subframe_rect.width; i++) {
            int fx = subframe_rect.x + i;
            if (fx < 0 || fx >= frame->width) continue;
            frame->data[fy * frame->width + fx] =
                rotated.data[(offset_y + j) * ext.width + (offset_x + i)];
        }
    }
    image_free(&src_subframe);
    image_free(&rotated);
}

uint8_t bilinear_interp_for_point(double x, double y, const Image* img) {
    int image_x_size = img->width;
    int image_y_size = img->height;
    uint8_t* data = img->data;
    if (x < 0.0) x = 0.0;
    if (x > image_x_size - 2) x = image_x_size - 2;
    if (y < 0.0) y = 0.0;
    if (y > image_y_size - 2) y = image_y_size - 2;
    int x1 = (int)x;
    int x2 = x1 + 1;
    int y1 = (int)y;
    int y2 = y1 + 1;
    int Ix1y1 = data[y1 * image_x_size + x1];
    int Ix2y1 = data[y1 * image_x_size + x2];
    int Ix1y2 = data[y2 * image_x_size + x1];
    int Ix2y2 = data[y2 * image_x_size + x2];
    double dx = x - x1;
    double dy = y - y1;
    double p1 = Ix1y1 + dx * (Ix2y1 - Ix1y1);
    double p2 = Ix1y2 + dx * (Ix2y2 - Ix1y2);
    int res = (int)(p1 + dy * (p2 - p1));
    if (res < 0) res = 0;
    if (res > 255) res = 255;
    return (uint8_t)res;
}

double degree2rad(double angle) {
    return angle * M_PI / 180.0;
}
double rad2degree(double angle) {
    return angle * 180.0 / M_PI;
}

int rect_area(Rect r) {
    return r.width * r.height;
}

void subframe_linear_transform(const Image* frame, Rect in_strobe, double angle,
                              double scale, int offset_x, int offset_y, Image* out) {
    Rect strobe = adjust_rect_to_frame(in_strobe, (Size){frame->width, frame->height});
    out->width = strobe.width;
    out->height = strobe.height;
    out->data = (uint8_t*)malloc(out->width * out->height);
    if (!out->data) { out->width = 0; out->height = 0; return; }
    angle = degree2rad(angle);
    int central_x_pix = strobe.x + strobe.width / 2;
    int central_y_pix = strobe.y + strobe.height / 2;
    int offs_x = strobe.x - central_x_pix;
    int offs_y = strobe.y - central_y_pix;
    double x_scaled, y_scaled, snan, csan, x_rotated, y_rotated;
    for (int j = 0; j < strobe.height; ++j) {
        for (int i = 0; i < strobe.width; ++i) {
            if (scale != 1.0) {
                x_scaled = (offs_x + i) / scale + central_x_pix;
                y_scaled = (offs_y + j) / scale + central_y_pix;
            } else {
                x_scaled = strobe.x + i;
                y_scaled = strobe.y + j;
            }
            if (fabs(angle) > 1e-9) {
                snan = sin(angle);
                csan = cos(angle);
                x_rotated = (x_scaled - central_x_pix) * csan + (y_scaled - central_y_pix) * snan + central_x_pix;
                y_rotated = (-1) * (x_scaled - central_x_pix) * snan + (y_scaled - central_y_pix) * csan + central_y_pix;
            } else {
                x_rotated = x_scaled;
                y_rotated = y_scaled;
            }
            x_rotated = x_rotated - offset_x;
            y_rotated = y_rotated - offset_y;
            out->data[j * strobe.width + i] = bilinear_interp_for_point(x_rotated, y_rotated, frame);
        }
    }
}

double compute_iou(Rect a, Rect b) {
    int intersection_x_min = (a.x > b.x) ? a.x : b.x;
    int intersection_y_min = (a.y > b.y) ? a.y : b.y;
    int intersection_x_max = ((a.x + a.width) < (b.x + b.width)) ? (a.x + a.width) : (b.x + b.width);
    int intersection_y_max = ((a.y + a.height) < (b.y + b.height)) ? (a.y + a.height) : (b.y + b.height);
    if ((intersection_x_max <= intersection_x_min) ||
        (intersection_y_max <= intersection_y_min))
        return 0.0;
    else {
        double intersection_area = (intersection_y_max - intersection_y_min) *
                                   (intersection_x_max - intersection_x_min);
        double a_area = rect_area(a);
        double b_area = rect_area(b);
        return intersection_area / (a_area + b_area - intersection_area);
    }
}

Point2f get_mean_shift(const Point2f* start, const Point2f* stop, size_t count) {
    Point2f acc = {0.0f, 0.0f};
    for (size_t i = 0; i < count; ++i) {
        acc.x += (stop[i].x - start[i].x);
        acc.y += (stop[i].y - start[i].y);
    }
    if (count > 0) {
        acc.x /= (float)count;
        acc.y /= (float)count;
    }
    return acc;
}

static double point2f_dist(Point2f a, Point2f b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx*dx + dy*dy);
}

static int compare_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

double get_scale(const Point2f* start, const Point2f* stop, size_t count) {
    size_t max_samples = count * (count - 1) / 2;
    double* scale_sample = malloc(max_samples * sizeof(double));
    size_t idx = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            double dist_prev = point2f_dist(start[i], start[j]);
            double dist_cur  = point2f_dist(stop[i], stop[j]);
            if (dist_prev > 1e-8)
                scale_sample[idx++] = sqrt((dist_cur * dist_cur) / (dist_prev * dist_prev));
        }
    }
    if (idx == 0) {
        free(scale_sample);
        return 1.0;
    }
    qsort(scale_sample, idx, sizeof(double), compare_double);
    double median = scale_sample[idx / 2];
    free(scale_sample);
    return median;
}

void draw_candidate(Image* img, Candidate c) {
    int value = 0;
    int thickness = 1;
    switch (c.src) {
        case PROPOSAL_SOURCE_TRACKER: value = 128; break;
        case PROPOSAL_SOURCE_DETECTOR: value = 200; break;
        case PROPOSAL_SOURCE_MIXED: value = 180; break;
        case PROPOSAL_SOURCE_FINAL:
            if (!c.valid) return;
            value = 255;
            thickness = (int)(4 * c.prob);
            if (thickness < 1) thickness = 1;
            break;
        default: value = 0; break;
    }
    int x1 = c.strobe.x, y1 = c.strobe.y;
    int x2 = c.strobe.x + c.strobe.width - 1;
    int y2 = c.strobe.y + c.strobe.height - 1;
    for (int t = 0; t < thickness; t++) {
        for (int x = x1 + t; x <= x2 - t; x++) {
            if (y1 + t >= 0 && y1 + t < img->height && x >= 0 && x < img->width)
                img->data[(y1 + t) * img->width + x] = value;
            if (y2 - t >= 0 && y2 - t < img->height && x >= 0 && x < img->width)
                img->data[(y2 - t) * img->width + x] = value;
        }
        for (int y = y1 + t; y <= y2 - t; y++) {
            if (x1 + t >= 0 && x1 + t < img->width && y >= 0 && y < img->height)
                img->data[y * img->width + (x1 + t)] = value;
            if (x2 - t >= 0 && x2 - t < img->width && y >= 0 && y < img->height)
                img->data[y * img->width + (x2 - t)] = value;
        }
    }
}

void draw_candidates(Image* img, Candidate* candidates, size_t num_candidates) {
    for (size_t i = 0; i < num_candidates; ++i)
        draw_candidate(img, candidates[i]);
}

void get_scan_position_cnt(Size frame_size, Size box, const double* scales, const Size* steps, size_t scales_count, Size* out) {
    for (size_t scale_id = 0; scale_id < scales_count; ++scale_id) {
        double scale = scales[scale_id];
        Size grid_size;
        Size scaled_bbox;
        scaled_bbox.width = (int)(box.width * scale);
        scaled_bbox.height = (int)(box.height * scale);
        int scanning_area_x = frame_size.width - scaled_bbox.width;
        int scanning_area_y = frame_size.height - scaled_bbox.height;
        int step_x = steps[scale_id].width;
        int step_y = steps[scale_id].height;
        grid_size.width = 1 + scanning_area_x / step_x;
        grid_size.height = 1 + scanning_area_y / step_y;
        out[scale_id] = grid_size;
    }
}

double images_correlation(const Image* image_1, const Image* image_2) {
    int n_pixels = image_1->width * image_1->height;
    double sum1 = 0.0, sum2 = 0.0;
    double sqsum1 = 0.0, sqsum2 = 0.0;
    for (int i = 0; i < n_pixels; ++i) {
        double v1 = (double)image_1->data[i];
        double v2 = (double)image_2->data[i];
        sum1 += v1;
        sum2 += v2;
        sqsum1 += v1 * v1;
        sqsum2 += v2 * v2;
    }
    double mean1 = sum1 / n_pixels;
    double mean2 = sum2 / n_pixels;
    double std1 = sqrt(sqsum1 / n_pixels - mean1 * mean1);
    double std2 = sqrt(sqsum2 / n_pixels - mean2 * mean2);
    double covar = 0.0;
    for (int i = 0; i < n_pixels; ++i) {
        double v1 = (double)image_1->data[i] - mean1;
        double v2 = (double)image_2->data[i] - mean2;
        covar += v1 * v2;
    }
    covar /= n_pixels;
    double correl = covar / (std1 * std2 + 1e-12);
    return correl;
}

void sort_candidates_desc(Candidate* arr, int count) {
    for (int i = 0; i < count-1; ++i) {
        for (int j = i+1; j < count; ++j) {
            if (arr[j].prob > arr[i].prob) {
                Candidate tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}

int non_max_suppression(const Candidate* in, int in_count, double threshold_iou, Candidate* out) {
    Candidate* sorted = malloc(sizeof(Candidate) * in_count);
    memcpy(sorted, in, sizeof(Candidate) * in_count);
    sort_candidates_desc(sorted, in_count);
    int out_count = 0;
    int* suppressed = calloc(in_count, sizeof(int));
    for (int i = 0; i < in_count; ++i) {
        if (suppressed[i]) continue;
        out[out_count++] = sorted[i];
        Rect ref_rect = sorted[i].strobe;
        for (int j = i + 1; j < in_count; ++j) {
            if (!suppressed[j]) {
                double iou = compute_iou(sorted[j].strobe, ref_rect);
                if (iou > threshold_iou) {
                    suppressed[j] = 1;
                }
            }
        }
    }
    free(sorted);
    free(suppressed);
    return out_count;
}

int clusterize_candidates(const Candidate* in, int in_count, double threshold_iou, Candidate* out) {
    int* clustered = calloc(in_count, sizeof(int));
    int cluster_count = 0;
    for (int i = 0; i < in_count; ++i) {
        if (clustered[i]) continue;
        int cluster_indices[in_count];
        int cluster_size = 0;
        cluster_indices[cluster_size++] = i;
        clustered[i] = 1;
        for (int j = 0; j < in_count; ++j) {
            if (i == j || clustered[j]) continue;
            int match = 1;
            for (int k = 0; k < cluster_size; ++k) {
                double iou = compute_iou(in[cluster_indices[k]].strobe, in[j].strobe);
                if (iou < threshold_iou) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                cluster_indices[cluster_size++] = j;
                clustered[j] = 1;
            }
        }
        Candidate cluster_arr[cluster_size];
        for (int k = 0; k < cluster_size; ++k)
            cluster_arr[k] = in[cluster_indices[k]];
        Candidate avg = aggregate_candidates(cluster_arr, cluster_size);
        avg.src = in[cluster_indices[0]].src;
        out[cluster_count++] = avg;
    }
    free(clustered);
    return cluster_count;
}

Candidate aggregate_candidates(const Candidate* sample, int sample_count) {
    Candidate out;
    out.prob = 0.0;
    out.aux_prob = 0.0;
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
    for (int i = 0; i < sample_count; ++i) {
        x += sample[i].strobe.x;
        y += sample[i].strobe.y;
        w += sample[i].strobe.width;
        h += sample[i].strobe.height;
        if (sample[i].prob > out.prob) out.prob = sample[i].prob;
        if (sample[i].aux_prob > out.aux_prob) out.aux_prob = sample[i].aux_prob;
    }
    out.strobe.x = (int)(x / sample_count);
    out.strobe.y = (int)(y / sample_count);
    out.strobe.width = (int)(w / sample_count);
    out.strobe.height = (int)(h / sample_count);
    out.src = PROPOSAL_SOURCE_MIXED;
    return out;
}

Rect adjust_rect_to_frame(Rect rect, Size sz) {
    Rect out;
    int x1 = rect.x;
    int y1 = rect.y;
    int x2 = x1 + rect.width;
    int y2 = y1 + rect.height;
    x1 = x1 < 0 ? 0 : (x1 > sz.width - 1 ? sz.width - 1 : x1);
    y1 = y1 < 0 ? 0 : (y1 > sz.height - 1 ? sz.height - 1 : y1);
    x2 = x2 < 0 ? 0 : (x2 > sz.width - 1 ? sz.width - 1 : x2);
    y2 = y2 < 0 ? 0 : (y2 > sz.height - 1 ? sz.height - 1 : y2);
    out.x = x1;
    out.y = y1;
    out.width = x2 - x1;
    out.height = y2 - y1;
    return out;
}

int strobe_is_outside(Rect rect, Size sz) {
    if (rect.x < 0 || rect.x >= sz.width ||
        rect.y < 0 || rect.y >= sz.height ||
        rect.x + rect.width < 0 || rect.x + rect.width >= sz.width ||
        rect.y + rect.height < 0 || rect.y + rect.height >= sz.height)
        return 1;
    return 0;
}

double get_frame_std_dev(const Image* frame, Rect roi) {
    double mean = 0.0, sum = 0.0, sqsum = 0.0;
    int count = 0;
    for (int j = roi.y; j < roi.y + roi.height; ++j) {
        for (int i = roi.x; i < roi.x + roi.width; ++i) {
            int idx = j * frame->width + i;
            double v = (double)frame->data[idx];
            sum += v;
            sqsum += v * v;
            ++count;
        }
    }
    if (count == 0) return 0.0;
    mean = sum / count;
    double variance = (sqsum / count) - (mean * mean);
    double stddev = sqrt(variance > 0.0 ? variance : 0.0);
    return stddev;
}

