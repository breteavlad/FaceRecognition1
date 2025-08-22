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
#include <map>
#include <opencv2/core/types_c.h>
#include <unistd.h> 

using namespace cv;
using namespace cv::face;
using namespace std;
namespace fs = std::filesystem;

// Global variables
Ptr<LBPHFaceRecognizer> model; // Face recognizer model
std::vector<Mat> images;        // Training images
std::vector<int> labels;        // Labels for training images
std::vector<std::string> names; // Names corresponding to labels

// Function to initialize video capture with fallback options
VideoCapture initializeCapture() {
    VideoCapture capture;
    
    // For Raspberry Pi, we need to use the named pipe approach
    cout << "Setting up Raspberry Pi camera with named pipe..." << endl;
    
    // First, try to create the named pipe if it doesn't exist
    system("mkfifo /tmp/vidpipe 2>/dev/null || true");
    
    // Start rpicam-vid in the background if not already running
    system("pkill rpicam-vid 2>/dev/null || true"); // Kill any existing instances
    system("rpicam-vid -t 0 --width 640 --height 480 --framerate 30 --output /tmp/vidpipe &");
    
    // Wait a moment for the pipe to be ready
    sleep(2);
    
    // Try to open the pipe with OpenCV
    cout << "Attempting to open video pipe: /tmp/vidpipe" << endl;
    
    // Try opening as a video file (pipe)
    capture.open("/tmp/vidpipe", CAP_FFMPEG);
    
    if (capture.isOpened()) {
        cout << "Successfully opened Raspberry Pi camera via named pipe" << endl;
        return capture;
    }
    
    cout << "Named pipe failed, trying standard camera access..." << endl;
    
    // Fallback: try standard camera indices
    for (int i = 0; i < 3; i++) {
        cout << "Trying camera index: " << i << endl;
        capture.open(i);
        
        if (capture.isOpened()) {
            cout << "Successfully opened camera index: " << i << endl;
            return capture;
        }
    }
    
    cerr << "Error: Could not open any video source" << endl;
    cerr << "Make sure rpicam-vid is available and camera is connected" << endl;
    return capture;
}

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
        if (doRecognize && !model.empty() && !model->empty()) {
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

void collectFaceSamples(CascadeClassifier& cascade, int label, const string& name) {
    VideoCapture capture = initializeCapture();
    if (!capture.isOpened()) {
        cerr << "Error opening video capture\n";
        return;
    }

    Mat frame, face;
    string folderPath = "faces/" + name;
    fs::create_directories(folderPath);

    int sampleCount = 0;
    const int MAX_SAMPLES = 20;

    cout << "Collecting face samples for " << name << ". Press 's' to save a sample, 'q' to quit (or wait for samples to auto-save).\n";

    while (sampleCount < MAX_SAMPLES) {
        capture >> frame;
        if (frame.empty()) {
            cerr << "Error: Blank frame\n";
            continue;
        }

        Mat gray;
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        equalizeHist(gray, gray);

        vector<Rect> faces;
        cascade.detectMultiScale(gray, faces, 1.1, 3, 0, Size(100, 100));

        if (!faces.empty()) {
            Rect largestFace = faces[0];
            for (const auto& r : faces) {
                if (r.area() > largestFace.area()) {
                    largestFace = r;
                }
            }

            Mat faceROI = gray(largestFace);
            resize(faceROI, faceROI, Size(100, 100));

            string filename = folderPath + "/sample_" + to_string(sampleCount) + ".jpg";
            imwrite(filename, faceROI);
            cout << "Saved " << filename << endl;

            sampleCount++;
            waitKey(500); // wait between frames
        }

        // Show current frame with face detection
        Mat displayFrame = frame.clone();
        vector<Rect> displayFaces;
        cascade.detectMultiScale(gray, displayFaces, 1.1, 3, 0, Size(100, 100));
        for (const auto& r : displayFaces) {
            rectangle(displayFrame, r, Scalar(0, 255, 0), 2);
        }
        imshow("Collecting Samples", displayFrame);

        char c = (char)waitKey(10);
        if (c == 'q' || c == 27) {
            break;
        }
    }

    destroyWindow("Collecting Samples");
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
        
        // Update label counter to avoid conflicts
        if (!names.empty()) {
            label = names.size();
        }
    }

    // Iterate through faces directory
    for (const auto& entry : fs::directory_iterator("faces")) {
        if (entry.is_directory()) {
            string name = entry.path().filename().string();

            // Assign a label if this person doesn't have one yet
            if (nameToLabel.find(name) == nameToLabel.end()) {
                nameToLabel[name] = label;
                if (names.size() <= label) {
                    names.resize(label + 1);
                }
                names[label] = name;
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

    // Save labels to file (FIXED: was saving to wrong path)
    ofstream outLabelFile("faces/labels.txt");
    if (outLabelFile.is_open()) {
        for (size_t i = 0; i < names.size(); i++) {
            if (!names[i].empty()) {
                outLabelFile << names[i] << " " << i << endl;
            }
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

    // Load cascades - try different possible paths
    CascadeClassifier cascade, nestedCascade;
    double scale = 1;

    vector<string> faceCascadePaths = {
        "/usr/share/opencv4/haarcascades/haarcascade_frontalface_alt.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_alt.xml",
        "haarcascade_frontalface_alt.xml"
    };

    vector<string> eyeCascadePaths = {
        "/usr/share/opencv4/haarcascades/haarcascade_eye_tree_eyeglasses.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_eye_tree_eyeglasses.xml",
        "haarcascade_eye_tree_eyeglasses.xml"
    };

    bool faceLoaded = false, eyeLoaded = false;

    for (const string& path : faceCascadePaths) {
        if (cascade.load(path)) {
            cout << "Loaded face cascade from: " << path << endl;
            faceLoaded = true;
            break;
        }
    }

    for (const string& path : eyeCascadePaths) {
        if (nestedCascade.load(path)) {
            cout << "Loaded eye cascade from: " << path << endl;
            eyeLoaded = true;
            break;
        }
    }

    if (!faceLoaded) {
        cerr << "ERROR: Could not load frontal face cascade from any path\n";
        return -1;
    }

    if (!eyeLoaded) {
        cerr << "WARNING: Could not load eye cascade - eye detection disabled\n";
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
            if (model.empty() || model->empty()) {
                if (!loadFaceRecognizer()) {
                    cout << "No trained model available. Train the recognizer first.\n";
                    break;
                }
            }

            VideoCapture capture = initializeCapture();
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