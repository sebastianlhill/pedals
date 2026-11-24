#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <cmath>
#include <algorithm> 
#include <thread> 
#include <iostream> 

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "window_manager.h"
#include "shader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "portaudio/pa_ringbuffer.h"
#include <portaudio.h> // PortAudio: Used for audio capture
#include <fftw3.h>     // FFTW:      Provides a discrete FFT algorithm to get
                       //            frequency data from captured audio

#define SAMPLE_RATE 44100.0   // How many audio samples to capture every second (44100 Hz is standard)
#define FRAMES_PER_BUFFER 512 // How many audio samples to send to our callback function for each channel
#define NUM_CHANNELS 2        // Number of audio channels to capture

#define SPECTRO_FREQ_START 20  // Lower bound of the displayed spectrogram (Hz)
#define SPECTRO_FREQ_END 20000 // Upper bound of the displayed spectrogram (Hz)


// ring buffer for our audio samples, 
// so we allocate a consitent amount of memory 
// for the sound overtime. this is shared between threads 
// buffer size must be power of two 
const long ringBufferSize = 4096; 

// for windowing 

int width = 800, height = 600;


// Define our callback data (data that is passed to every callback function call)
typedef struct {
    double* in;      // Input buffer, will contain our audio sample
    double* out;     // Output buffer, FFTW will write to this based on the input buffer's contents
    //fftw_plan p;     // Created by FFTW to facilitate FFT calculation
    int startIndex;  // First index of our FFT output to display in the spectrogram
    int spectroSize; // Number of elements in our FFT output to display from the start index
                     //
    PaUtilRingBuffer ringBuffer; 
    float*           ringData;
} streamCallbackData;

struct visualizer {
  GLuint fbo = 0, vizTexture = 0;
  static constexpr int VIZ_WIDTH = 1024;
  static constexpr int VIZ_HEIGHT = 256;
}

struct audioTex {
  GLuint tex; 
  static constexpr int AUDIO_SAMPLES = 1024;
}

static Visualizer gViz;
static audioTex   gAudio;

void initViz(Visualizer& v) {
  glGenFramebuffers(1, &v.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, v.fbo);

  glGenTextures(1, &v.vizTexture);
  glBindTexture(GL_TEXTURE_2D, v.vizTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v.VIZ_WIDTH, v.VIZ_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_FILTER, GL_LINEAR);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, v.vizTexture, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initAudioTex() {
  glGenTextures(1, &v.audioTex);
  glBindTexture(GL_TEXTURE_1D, v.audioTex);
      glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F,
                 AudioTex::AUDIO_SAMPLES, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

// Callback data, persisted between calls. Allows us to access the data it
// contains from within the callback function.
static streamCallbackData* userData;

// Checks the return value of a PortAudio function. Logs the message and exits
// if there was an error
static void checkErr(PaError err) {
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}

// Returns the lowest of the two given numbers
static inline float min(float a, float b) {
    return a < b ? a : b;
}

// PortAudio stream callback function. Will be called after every
// `FRAMES_PER_BUFFER` audio samples PortAudio captures. Used to process the
// resulting audio sample.
static int streamCallback(
    const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
    void* userData 
) {

    float* out = (float*)outputBuffer;
    float* in = (float*)inputBuffer;
    
    static float lp = 0.0f; 
    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        float sample = in[i * NUM_CHANNELS]; // left channel (mono guitar)
        
        sample *= 1.0f;
        // gate 
        float gate = 0.00f; 
        float env = std::abs(sample);
        if (env < gate) sample = 0.0f;

        float drive = 20.0f; 

        float y = std::tanh(sample * drive);
            
        // remove static 
        lp += 0.2f * (y - lp);
        y = lp;

        // Copy to both output channels
        out[i * NUM_CHANNELS]     = y;
        out[i * NUM_CHANNELS + 1] = y;
    }
    streamCallbackData* data = (streamCallbackData*)userData; 
    PaUtil_WriteRingBuffer(&data->ringBuffer, out, framesPerBuffer * NUM_CHANNELS);

    return 0;
}
void display(int id) {

}



int main() {
    // Initialize PortAudio
    PaError err;
    err = Pa_Initialize();
    checkErr(err);
    
    // allocate our user data 

    userData = (streamCallbackData*)calloc(1, sizeof(streamCallbackData));
    if (!userData) {exit(EXIT_FAILURE); } 


    userData->ringData = (float*)malloc(ringBufferSize * sizeof(float));
    PaUtil_InitializeRingBuffer(&userData->ringBuffer, sizeof(float), ringBufferSize, userData->ringData);
   
    // Get and display the number of audio devices accessible to PortAudio
    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0) {
        printf("Error getting device count.\n");
        exit(EXIT_FAILURE);
    } else if (numDevices == 0) {
        printf("There are no available audio devices on this machine.\n");
        exit(EXIT_SUCCESS);
    }

    // Display audio device information for each device accessible to PortAudio
    const PaDeviceInfo* deviceInfo;
    bool debug = false;
    if (debug){
    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        printf("Device %d:\n", i);
        printf("  name: %s\n", deviceInfo->name);
        printf("  maxInputChannels: %d\n", deviceInfo->maxInputChannels);
        printf("  maxOutputChannels: %d\n", deviceInfo->maxOutputChannels);
        printf("  defaultSampleRate: %f\n", deviceInfo->defaultSampleRate);
    }
    }
    // Use device 0 (for a programmatic solution for choosing a device,
    // `numDevices - 1` is typically the 'default' device
    int device = Pa_GetDefaultInputDevice();
    if (device == paNoDevice) {
        printf("No default input device.\n");
        exit(EXIT_FAILURE);
    }
    printf("Using device %d: %s\n", device, Pa_GetDeviceInfo(device)->name);

    // Define stream capture specifications
    PaStreamParameters inputParameters;
    memset(&inputParameters, 0, sizeof(inputParameters));
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.device = device;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowInputLatency;

    PaStreamParameters outputParameters;
    memset(&outputParameters, 0, sizeof(outputParameters));
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    
    PaStream* stream;
    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paNoFlag,
        streamCallback,
        userData
    );
    checkErr(err);
    
      
    // Begin capturing audio
    err = Pa_StartStream(stream);
    checkErr(err);


    // windowing 
    try {

        WindowManager wm(width,height, "pedals", /*fullscreen*/ false);
        //GLFWwindow* window = wm.getWindow();

        while (!wm.shouldClose())
        {
          wm.pollEvents();
          glClear(GL_COLOR_BUFFER_BIT);

          


          wm.beginImGuiFrame();
          bool show_demo_window = true; 
          ImGui::ShowDemoWindow(&show_demo_window);
          
          // imgui 
          ImGui::Begin("Guitar");
          ImGui::Text("Drive");
          ImGui::End();

          wm.renderImGui();
          wm.swapBuffers();
        }
      } catch (const std::exception& e) {
        std::cerr << "Error" << e.what() << std::endl;
      }
    

    // Stop capturing audio
    err = Pa_StopStream(stream);
    err = Pa_CloseStream(stream);
    err = Pa_Terminate();
    checkErr(err);
    
    // free memory 
    free(userData->ringData);
    free(userData);
    
    printf("\n");
  
    return EXIT_SUCCESS;
}
