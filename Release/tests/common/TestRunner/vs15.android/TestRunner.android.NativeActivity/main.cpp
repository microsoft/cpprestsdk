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
*
*/

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

/**
* Our saved state data.
*/
struct saved_state {
    float angle;
    int32_t x;
    int32_t y;
};

/**
* Shared state for our app.
*/
struct engine {
    struct android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    int animating;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct saved_state state;
};

/**
* Initialize an EGL context for the current display.
*/
static int engine_init_display(struct engine* engine) {
    // initialize OpenGL ES and EGL

    /*
    * Here specify the attributes of the desired configuration.
    * Below, we select an EGLConfig with at least 8 bits per color
    * component compatible with on-screen windows
    */
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_NONE
    };
    EGLint w, h, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
    * sample, we have a very simplified selection process, where we pick
    * the first EGLConfig that matches our criteria */
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
    * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
    * As soon as we picked a EGLConfig, we can safely reconfigure the
    * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    context = eglCreateContext(display, config, NULL, NULL);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;
    engine->state.angle = 0;

    // Initialize GL state.
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    return 0;
}

/**
* Just the current frame in the display.
*/
static void engine_draw_frame(struct engine* engine) {
    if (engine->display == NULL) {
        // No display.
        return;
    }

    // Just fill the screen with a color.
    glClearColor(((float)engine->state.x) / engine->width, engine->state.angle,
        ((float)engine->state.y) / engine->height, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    eglSwapBuffers(engine->display, engine->surface);
}

/**
* Tear down the EGL context currently associated with the display.
*/
static void engine_term_display(struct engine* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->animating = 0;
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

/**
* Process the next input event.
*/
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* engine = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        engine->state.x = AMotionEvent_getX(event, 0);
        engine->state.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/**
* Process the next main command.
*/
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
    case APP_CMD_SAVE_STATE:
        // The system has asked us to save our current state.  Do so.
        engine->app->savedState = malloc(sizeof(struct saved_state));
        *((struct saved_state*)engine->app->savedState) = engine->state;
        engine->app->savedStateSize = sizeof(struct saved_state);
        break;
    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
        if (engine->app->window != NULL) {
            engine_init_display(engine);
            engine_draw_frame(engine);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        // The window is being hidden or closed, clean it up.
        engine_term_display(engine);
        break;
    case APP_CMD_GAINED_FOCUS:
        // When our app gains focus, we start monitoring the accelerometer.
        if (engine->accelerometerSensor != NULL) {
            ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                engine->accelerometerSensor);
            // We'd like to get 60 events per second (in us).
            ASensorEventQueue_setEventRate(engine->sensorEventQueue,
                engine->accelerometerSensor, (1000L / 60) * 1000);
        }
        break;
    case APP_CMD_LOST_FOCUS:
        // When our app loses focus, we stop monitoring the accelerometer.
        // This is to avoid consuming battery while not being used.
        if (engine->accelerometerSensor != NULL) {
            ASensorEventQueue_disableSensor(engine->sensorEventQueue,
                engine->accelerometerSensor);
        }
        // Also stop animating.
        engine->animating = 0;
        engine_draw_frame(engine);
        break;
    }
}

#include <jni.h>
#include <pthread.h>
#include <string>
#include <sstream>
#include <mutex>
#include <functional>
#include "unittestpp.h"
#include "src/TestReporter.h"
#include "src/TestDetails.h"
#include <pplx/pplxtasks.h>
#include <pplx/threadpool.h>
#include <android/log.h>

void printLn(const std::string& s) {
    __android_log_print(ANDROID_LOG_WARN, "UnitTestpp", "%s", s.c_str());
}

struct MyTestReporter : UnitTest::TestReporter {
    UNITTEST_LINKAGE virtual void ReportTestStart(UnitTest::TestDetails const& test) {
        std::stringstream ss;
        ss << test.suiteName << ":" << test.testName << ": Start.";
        printLn(ss.str());
    }
    UNITTEST_LINKAGE virtual void ReportFailure(UnitTest::TestDetails const& test, char const* failure) {
        std::stringstream ss;
        ss << test.suiteName << ":" << test.testName << ": " << failure;
        printLn(ss.str());
    }
    UNITTEST_LINKAGE virtual void ReportTestFinish(UnitTest::TestDetails const& test, bool passed, float secondsElapsed) {
        if (!passed) {
            std::stringstream ss;
            ss << test.suiteName << ":" << test.testName << ": Failed. Seconds: " << secondsElapsed;
            printLn(ss.str());
        }
    }
    UNITTEST_LINKAGE virtual void ReportSummary(int totalTestCount, int failedTestCount, int failureCount, float secondsElapsed) {
        std::stringstream ss;
        ss << "Tests complete. Total: " << totalTestCount << ", Failed: " << failedTestCount << ", Time: " << secondsElapsed;
        printLn(ss.str());
        // Print a bunch of messages to defeat any batching that may be applied by adb or logcat
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
        printLn("--- Flush buffer ---");
    }
    UNITTEST_LINKAGE virtual void print(const std::string& s) {
        printLn(s);
    }
};

static std::string to_lower(const std::string &str)
{
    std::string lower;
    for (auto iter = str.begin(); iter != str.end(); ++iter)
    {
        lower.push_back((char)tolower(*iter));
    }
    return lower;
}

bool matched_properties(UnitTest::TestProperties const& test_props) {
    if (test_props.Has("Requires")) {
        std::string const requires = test_props.Get("Requires");
        std::vector<std::string> requirements;

        // Can be multiple requirements, a semi colon seperated list
        std::string::size_type pos = requires.find_first_of(';');
        std::string::size_type last_pos = 0;
        while (pos != std::string::npos)
        {
            requirements.push_back(requires.substr(last_pos, pos - last_pos));
            last_pos = pos + 1;
            pos = requires.find_first_of(';', last_pos);
        }
        requirements.push_back(requires.substr(last_pos));
        for (auto iter = requirements.begin(); iter != requirements.end(); ++iter)
        {
            if (!UnitTest::GlobalSettings::Has(to_lower(*iter)))
            {
                return false;
            }
        }
    }
    return true;
}

bool should_run_test(UnitTest::Test *pTest)
{
    if (pTest->m_properties.Has("Ignore")) return false;
    if (pTest->m_properties.Has("Ignore:Linux")) return false;
    if (pTest->m_properties.Has("Ignore:Android")) return false;
    if (matched_properties(pTest->m_properties)) {
        return true;
    }
    return false;
}

void* RunTests() {
    UnitTest::TestList& tests = UnitTest::GetTestList();

    MyTestReporter mtr;

    // Do work
    UnitTest::TestRunner testrunner(mtr, false);
    testrunner.RunTestsIf(tests,
        [](UnitTest::Test *pTest) -> bool
    {
        if (should_run_test(pTest))
            return true;
        auto& test = pTest->m_details;
        std::stringstream ss;
        ss << test.suiteName << ":" << test.testName << ": Skipped.";
        printLn(ss.str());
        return false;
    }, 0);

    return nullptr;
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
void android_main(struct android_app* state) {
    struct engine engine;

    cpprest_init(state->activity->vm);

    // Begin change path to temp dir
    jobject nAct = state->activity->clazz;
    auto env = crossplat::get_jvm_env();

    auto contextClass = env->FindClass("android/content/Context");
    auto getCacheDir = env->GetMethodID(contextClass, "getCacheDir", "()Ljava/io/File;");

    auto fileClass = env->FindClass("java/io/File");
    auto getPath = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");

    auto cacheDir = env->CallObjectMethod(nAct, getCacheDir);
    jstring cacheDirPath = (jstring)env->CallObjectMethod(cacheDir, getPath);
    auto st = env->GetStringUTFChars(cacheDirPath, nullptr);
    chdir(st);
    env->ReleaseStringUTFChars(cacheDirPath, st);
    // End change path to temp dir

    RunTests();

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    engine.app = state;

    // Prepare to monitor accelerometer
    engine.sensorManager = ASensorManager_getInstance();
    engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
        ASENSOR_TYPE_ACCELEROMETER);
    engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
        state->looper, LOOPER_ID_USER, NULL, NULL);

    if (state->savedState != NULL) {
        // We are starting with a previous saved state; restore from it.
        engine.state = *(struct saved_state*)state->savedState;
    }

    engine.animating = 1;

    // loop waiting for stuff to do.

    while (1) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events,
            (void**)&source)) >= 0) {

            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                engine_term_display(&engine);
                return;
            }
        }

        if (engine.animating) {
            // Done with events; draw next animation frame.
            engine.state.angle += .01f;
            if (engine.state.angle > 1) {
                engine.state.angle = 0;
            }

            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            engine_draw_frame(&engine);
        }
    }
}
