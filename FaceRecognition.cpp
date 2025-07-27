#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/face.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <opencv2/core/types_c.h>

using namespace cv;
using namespace cv::face;
using namespace std;
namespace fs = std::filesystem;

// Global variables
Ptr<LBPHFaceRecognizer> model; // Face recognizer model
std::vector<Mat> images;        // Training images
std::vector<int> labels;        // Labels for training images
std::vector<std::string> names; // Names corresponding to labels



// Function to detect and draw faces
void detectAndDraw(Mat& img, CascadeClassifier& cascade,
    CascadeClassifier& nestedCascade,
    double scale, bool doRecognize = true)
{
    std::vector<Rect> faces;
    Mat gray, smallImg;
    cvtColor(img, gray, COLOR_BGR2GRAY); // Convert to Gray Scale
    double fx = 1 / scale;
    // Resize the Grayscale Image 
    resize(gray, smallImg, Size(), fx, fx, INTER_LINEAR);
    equalizeHist(smallImg, smallImg);
    // Detect faces of different sizes using cascade classifier 
    cascade.detectMultiScale(smallImg, faces, 1.1,
        2, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));

    // Draw circles around the faces
    for (size_t i = 0; i < faces.size(); i++)
    {
        Rect r = faces[i];
        Mat smallImgROI;
        std::vector<Rect> nestedObjects;
        Point center;
        Scalar color = Scalar(255, 0, 0); // Color for Drawing tool
        int radius;
        double aspect_ratio = (double)r.width / r.height;

        // Recognition - predict who this face belongs to
        string name = "Unknown";
        if (doRecognize && !model->empty()) {
            // Extract face ROI
            Mat faceROI = smallImg(r);

            // Predict
            int predictedLabel = -1;
            double confidence = 0.0;
            model->predict(faceROI, predictedLabel, confidence);

            // If confidence is low enough, consider it a match
            if (confidence < 100.0 && predictedLabel >= 0 && predictedLabel < names.size()) {
                name = names[predictedLabel];
                // More confident = green, less confident = red
                color = Scalar(0, 255 - confidence * 2.55, confidence * 2.55);
            }

            // Display name and confidence
            String box_text = name + " (" + std::to_string(int(confidence)) + ")";
            int pos_y = std::max(r.y - 10, 0) * scale;
            putText(img, box_text, Point(r.x * scale, pos_y),
                FONT_HERSHEY_SIMPLEX, 0.8, color, 2);
        }

        if (0.75 < aspect_ratio && aspect_ratio < 1.3)
        {
            center.x = cvRound((r.x + r.width * 0.5) * scale);
            center.y = cvRound((r.y + r.height * 0.5) * scale);
            radius = cvRound((r.width + r.height) * 0.25 * scale);
            circle(img, center, radius, color, 3, 8, 0);
        }
        else
            rectangle(img, cvPoint(cvRound(r.x * scale), cvRound(r.y * scale)),
                cvPoint(cvRound((r.x + r.width - 1) * scale),
                    cvRound((r.y + r.height - 1) * scale)), color, 3, 8, 0);

        if (nestedCascade.empty())
            continue;
        smallImgROI = smallImg(r);
        // Detection of eyes in the input image
        nestedCascade.detectMultiScale(smallImgROI, nestedObjects, 1.1, 2,
            0 | CASCADE_SCALE_IMAGE, Size(30, 30));
        // Draw circles around eyes
        for (size_t j = 0; j < nestedObjects.size(); j++)
        {
            Rect nr = nestedObjects[j];
            center.x = cvRound((r.x + nr.x + nr.width * 0.5) * scale);
            center.y = cvRound((r.y + nr.y + nr.height * 0.5) * scale);
            radius = cvRound((nr.width + nr.height) * 0.25 * scale);
            circle(img, center, radius, color, 3, 8, 0);
        }
    }
    // Show Processed Image with detected faces
    imshow("Face Recognition", img);
}

// Function to collect face samples for training
void collectFaceSamples(CascadeClassifier& cascade, int label, const string& name) {
    VideoCapture capture(0);
    if (!capture.isOpened()) {
        cerr << "Error opening video capture\n";
        return;
    }

    Mat frame, face;
    string folderPath = "faces/" + name;
    fs::create_directories(folderPath);

    int sampleCount = 0;
    const int MAX_SAMPLES = 20; // Collect 20 samples per person

    cout << "Collecting face samples for " << name << ". Press 's' to save a sample, 'q' to quit.\n";

    while (sampleCount < MAX_SAMPLES) {
        capture >> frame;
        if (frame.empty()) {
            cerr << "Error: Blank frame\n";
            break;
        }

        Mat gray;
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        equalizeHist(gray, gray);

        vector<Rect> faces;
        cascade.detectMultiScale(gray, faces, 1.1, 3, 0, Size(100, 100));

        Mat frameClone = frame.clone();

        for (const auto& r : faces) {
            rectangle(frameClone, r, Scalar(0, 255, 0), 2);
        }

        imshow("Collecting Faces", frameClone);

        char c = (char)waitKey(10);
        if (c == 's' && !faces.empty()) {
            // Save the largest face (assuming it's the user's face)
            Rect largestFace = faces[0];
            for (const auto& r : faces) {
                if (r.area() > largestFace.area()) {
                    largestFace = r;
                }
            }

            Mat faceROI = gray(largestFace);
            // Resize to a common size for training
            resize(faceROI, faceROI, Size(100, 100));

            string filename = folderPath + "/sample_" + to_string(sampleCount) + ".jpg";
            imwrite(filename, faceROI);
            cout << "Saved " << filename << endl;

            sampleCount++;
        }
        else if (c == 'q' || c == 27) {
            break;
        }
    }

    destroyWindow("Collecting Faces");
    cout << "Collected " << sampleCount << " samples for " << name << endl;
}

// Function to load training data from faces directory
bool loadTrainingData() {
    images.clear();
    labels.clear();
    names.clear();

    if (!fs::exists("faces")) {
        cerr << "Faces directory not found. Create it first.\n";
        return false;
    }

    int label = 0;
    map<string, int> nameToLabel;

    // Load existing labels from file if it exists
    ifstream labelFile("faces/labels.txt");
    if (labelFile.is_open()) {
        string name;
        int lbl;
        while (labelFile >> name >> lbl) {
            nameToLabel[name] = lbl;
            while (names.size() <= lbl) {
                names.push_back("");
            }
            names[lbl] = name;
        }
        labelFile.close();
    }

    // Iterate through faces directory
    for (const auto& entry : fs::directory_iterator("faces")) {
        if (entry.is_directory()) {
            string name = entry.path().filename().string();

            // Assign a label if this person doesn't have one yet
            if (nameToLabel.find(name) == nameToLabel.end()) {
                nameToLabel[name] = label;
                names.push_back(name);
                label++;
            }

            int personLabel = nameToLabel[name];

            // Load all face samples for this person
            for (const auto& sample : fs::directory_iterator(entry.path())) {
                if (sample.path().extension() == ".jpg" || sample.path().extension() == ".png") {
                    Mat img = imread(sample.path().string(), IMREAD_GRAYSCALE);
                    if (!img.empty()) {
                        // Resize to ensure consistent size
                        resize(img, img, Size(100, 100));
                        images.push_back(img);
                        labels.push_back(personLabel);
                    }
                }
            }
        }
    }

    // Save labels to file
    ofstream outLabelFile("faces/Me/labels.txt");
    if (outLabelFile.is_open()) {
        for (size_t i = 0; i < names.size(); i++) {
            outLabelFile << names[i] << " " << i << endl;
        }
        outLabelFile.close();
    }

    cout << "Loaded " << images.size() << " images for " << names.size() << " people\n";

    return !images.empty();
}

// Function to train the face recognizer
bool trainFaceRecognizer() {
    if (images.empty() || labels.empty()) {
        cerr << "No training data available\n";
        return false;
    }

    // Create and train the LBPH Face Recognizer
    model = LBPHFaceRecognizer::create();
    model->train(images, labels);

    cout << "Face recognizer trained successfully\n";

    // Save the model
    model->save("faces/face_model.yml");
    cout << "Model saved to faces/face_model.yml\n";

    return true;
}

// Function to load a trained model if it exists
bool loadFaceRecognizer() {
    model = LBPHFaceRecognizer::create();
    try {
        model->read("faces/face_model.yml");
        cout << "Loaded trained model from faces/face_model.yml\n";
        return true;
    }
    catch (const cv::Exception& e) {
        cerr << "Error loading face model: " << e.what() << endl;
        return false;
    }
}

int main(int argc, const char** argv) {
    cout << "Face Recognition System" << endl;

    // Load cascades
    CascadeClassifier cascade, nestedCascade;
    double scale = 1;

    if (!cascade.load("C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/buildtrees/opencv4/src/4.11.0-46ecfbc8ae.clean/data/haarcascades/haarcascade_frontalface_alt.xml")) {
        cerr << "ERROR: Could not load frontal face\n";
        return -1;
    }

    if (!nestedCascade.load("C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/buildtrees/opencv4/src/4.11.0-46ecfbc8ae.clean/data/haarcascades/haarcascade_eye_tree_eyeglasses.xml")) {
        cerr << "ERROR: Could not load eye cascade\n";
        return -1;
    }

    // Create faces directory if it doesn't exist
    fs::create_directories("faces");

    // Load existing training data and model if available
    loadTrainingData();
    loadFaceRecognizer();

    // Main menu
    while (true) {
        cout << "\nFace Recognition System\n";
        cout << "1. Add new person\n";
        cout << "2. Train recognizer\n";
        cout << "3. Start recognition\n";
        cout << "4. Exit\n";
        cout << "Choose an option: ";

        int choice;
        cin >> choice;

        switch (choice) {
        case 1: {
            cin.ignore();
            cout << "Enter person's name: ";
            string name;
            getline(cin, name);

            int newLabel = names.size();
            collectFaceSamples(cascade, newLabel, name);

            // Reload training data
            loadTrainingData();
            break;
        }
        case 2: {
            if (loadTrainingData()) {
                trainFaceRecognizer();
            }
            else {
                cout << "No training data available. Add faces first.\n";
            }
            break;
        }
        case 3: {
            // Make sure we have a trained model
            if (model->empty() && !loadFaceRecognizer()) {
                cout << "No trained model available. Train the recognizer first.\n";
                break;
            }

            VideoCapture capture(0);
            if (!capture.isOpened()) {
                cerr << "Error opening video capture\n";
                break;
            }

            cout << "Face Recognition Started... Press 'q' to quit\n";

            Mat frame;
            while (true) {
                capture >> frame;
                if (frame.empty()) {
                    cerr << "Error: Blank frame\n";
                    break;
                }

                Mat frameClone = frame.clone();
                detectAndDraw(frameClone, cascade, nestedCascade, scale, true);

                char c = (char)waitKey(10);
                if (c == 'q' || c == 27) {
                    break;
                }
            }

            destroyWindow("Face Recognition");
            break;
        }
        case 4:
            cout << "Exiting...\n";
            return 0;
        default:
            cout << "Invalid option. Try again.\n";
        }
    }

    return 0;
}