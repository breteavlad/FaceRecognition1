#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/face.hpp>
#include <iostream>
#include <filesystem>
#include <opencv2/core/types_c.h>
using namespace cv;
using namespace cv::face;

Ptr<LBPHFaceRecognizer> model;
std::vector<std::string> names;

void detectAndRecognizeFace(Mat& img, CascadeClassifier cascade, CascadeClassifier nestedCascade, double scale) {
	//TODO: draw circle around face
	std::vector<Rect> faces;
	Mat gray, smallImg;
	double fx = 1 / scale;
	Scalar color = Scalar(255, 0, 0);
	double aspect_ratio;
	Point center;
	int radius;


	//recolor and resize the image, use histograms for future analysis 
	cvtColor(img, gray, COLOR_BGR2GRAY);
	resize(gray, smallImg, Size(), fx, fx);
	equalizeHist(smallImg, smallImg);
	cascade.detectMultiScale(smallImg, faces, 1.1, 3, 0, Size(15, 15));
	//Draw circles around face:
	for (size_t i = 0;i < faces.size();i++) {
		Rect r = faces[i];
		Mat smallImgROI;
		std::vector<Rect> nestedObjects;
		nestedCascade.detectMultiScale(img, nestedObjects, 1.1, 3, 0, Size(15, 15));
		//get the matrix with the face.
		Mat faceROI = smallImg(r);
		int predictLabel = -1;
		double confidence;
		String name = "Unknown";
		aspect_ratio = r.width / r.height;
		model->predict(faceROI, predictLabel, confidence);
		if (confidence < 100.0 && predictLabel >= 0 && predictLabel < names.size()) {
			name = names[predictLabel];
			color = Scalar(0, 255 - 2.55 * confidence, 2.55 * confidence);
		}
		String box_text = name + "( " + std::to_string(confidence) + " )";

		putText(img, box_text, Point(r.x * scale, r.y * scale), FONT_HERSHEY_COMPLEX, 0.2, Scalar(255, 0, 0));

		//use aspect ratio used for faces
		if (0.75 < aspect_ratio < 1.25) {
			center.x = cvRound((r.x + r.width) * 0.5) * scale;
			center.y = cvRound((r.y + r.height) * 0.5) * scale;
			radius = cvRound((r.width + r.height) * 0.25 * scale);
			circle(img, center, radius, color, 1, 8, 0);
		}


	}




}




int main() {
	//TODO: show the face on the screen in real-time
	VideoCapture capture;
	capture.open(0);
	Mat frame;
	CascadeClassifier cascade;
	if (!cascade.load("C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/buildtrees/opencv4/src/4.11.0-46ecfbc8ae.clean/data/haarcascades/haarcascade_frontalface_alt.xml")) {
		std::cerr << "Couldn't load the frontalfaces images \n";
		return -1;
	}

	CascadeClassifier nestedCascade;
	if (!nestedCascade.load("C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/buildtrees/opencv4/src/4.11.0-46ecfbc8ae.clean/data/haarcascades/haarcascade_frontalface_alt.xml")) {
		std::cerr << "Couldn't load the frontalfaces images \n";
		return -1;
	}




	while (capture.isOpened()) {
		capture >> frame;
		detectAndRecognizeFace(frame, cascade, nestedCascade, 1);

		imshow("Face Recognizer", frame);
		int k = waitKey(10);
		if (k == 'q') {
			break;
		}

	}





}