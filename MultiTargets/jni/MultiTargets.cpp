/*==============================================================================
            Copyright (c) 2010-2011 QUALCOMM Incorporated.
            All Rights Reserved.
            Qualcomm Confidential and Proprietary
            
@file 
    MultiTargets.cpp

@brief
    Sample for MultiTargets

==============================================================================*/


#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <QCAR/QCAR.h>
#include <QCAR/CameraDevice.h>
#include <QCAR/Renderer.h>
#include <QCAR/VideoBackgroundConfig.h>
#include <QCAR/Trackable.h>
#include <QCAR/Tool.h>
#include <QCAR/Tracker.h>
#include <QCAR/ImageTarget.h>
#include <QCAR/MultiTarget.h>
#include <QCAR/CameraCalibration.h>
#include <QCAR/UpdateCallback.h>

#include "SampleUtils.h"
#include "Texture.h"
#include "CubeShaders.h"
#include "Cube.h"
#include "BowlAndSpoonModel.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Textures:
int textureCount                = 0;
Texture** textures              = 0;

// OpenGL ES 2.0 specific:
unsigned int shaderProgramID    = 0;
GLint vertexHandle              = 0;
GLint normalHandle              = 0;
GLint textureCoordHandle        = 0;
GLint mvpMatrixHandle           = 0;

// Screen dimensions:
unsigned int screenWidth        = 0;
unsigned int screenHeight       = 0;

// Indicates whether screen is in portrait (true) or landscape (false) mode
bool isActivityInPortraitMode   = false;

// The projection matrix used for rendering virtual objects:
QCAR::Matrix44F projectionMatrix;

// Constants:
static const float kCubeScaleX    = 120.0f * 0.75f / 2.0f;
static const float kCubeScaleY    = 120.0f * 1.00f / 2.0f;
static const float kCubeScaleZ    = 120.0f * 0.50f / 2.0f;

static const float kBowlScaleX    = 120.0f * 0.15f;
static const float kBowlScaleY    = 120.0f * 0.15f;
static const float kBowlScaleZ    = 120.0f * 0.15f;


void initMIT();
void animateBowl(QCAR::Matrix44F& modelViewMatrix);


QCAR::MultiTarget* mit = NULL;


// Here we define a call-back that is executed every frame right after the
// Tracker finished its work. This is the ideal place to modify trackables.
// Always be sure to not try modifying something that was part of the state,
// since state objects cannot be modified. Doing this will crash your
// application.
//
struct MyUpdateCallBack : public QCAR::UpdateCallback
{
    virtual void QCAR_onUpdate(QCAR::State& state)
    {
        // Comment in the following lines to remove the bottom part of the
        // box at run-time. The first time this is executed, it will actually
        // work. After that the box has only five parts and the call will be
        // ignored (returning false).
        // 
        //if(mit!=NULL)
        //{
        //    mit->removePart(5);
        //}
    }
} myUpdateCallBack;


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_setActivityPortraitMode(JNIEnv *, jobject, jboolean isPortrait)
{
    isActivityInPortraitMode = isPortrait;
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_onQCARInitializedNative(JNIEnv *, jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_onQCARInitializedNative");

    initMIT();

    QCAR::registerCallback(&myUpdateCallBack);
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargetsRenderer_renderFrame(JNIEnv *, jobject)
{
    //LOG("Java_com_qualcomm_QCARSamples_MultiTargets_GLRenderer_renderFrame");
    SampleUtils::checkGlError("Check gl errors prior render Frame");

    // Clear color and depth buffer 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render video background:
    QCAR::State state = QCAR::Renderer::getInstance().begin();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Did we find any trackables this frame?
    if (state.getNumActiveTrackables())
    {
        // Get the trackable:
        const QCAR::Trackable* trackable=NULL;
        int numTrackables=state.getNumActiveTrackables();

        // Browse trackables searching for the MultiTarget
        for (int j=0;j<numTrackables;j++)
        {
            trackable = state.getActiveTrackable(j);
            if (trackable->getType() == QCAR::Trackable::MULTI_TARGET) break;
            trackable=NULL;
        }

        // If it was not found exit
        if (trackable==NULL)
        {
            // Clean up and leave
            glDisable(GL_BLEND);
            glDisable(GL_DEPTH_TEST);

            QCAR::Renderer::getInstance().end();
            return;
        }


        QCAR::Matrix44F modelViewMatrix =
            QCAR::Tool::convertPose2GLMatrix(trackable->getPose());        
        QCAR::Matrix44F modelViewProjection;
        SampleUtils::scalePoseMatrix(kCubeScaleX, kCubeScaleY, kCubeScaleZ,
                                     &modelViewMatrix.data[0]);
        SampleUtils::multiplyMatrix(&projectionMatrix.data[0],
                                    &modelViewMatrix.data[0],
                                    &modelViewProjection.data[0]);

        glUseProgram(shaderProgramID);
         
        // Draw the cube:

        glEnable(GL_CULL_FACE);

        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &cubeVertices[0]);
        glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &cubeNormals[0]);
        glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &cubeTexCoords[0]);
        
        glEnableVertexAttribArray(vertexHandle);
        glEnableVertexAttribArray(normalHandle);
        glEnableVertexAttribArray(textureCoordHandle);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]->mTextureID);
        glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );
        glDrawElements(GL_TRIANGLES, NUM_CUBE_INDEX, GL_UNSIGNED_SHORT,
                       (const GLvoid*) &cubeIndices[0]);

        glDisable(GL_CULL_FACE);

        // Draw the bowl:
        modelViewMatrix = QCAR::Tool::convertPose2GLMatrix(trackable->getPose());  

        // Remove the following line to make the bowl stop spinning:
        animateBowl(modelViewMatrix);

        SampleUtils::translatePoseMatrix(0.0f, -0.50f*120.0f, 1.35f*120.0f,
                                         &modelViewMatrix.data[0]);
        SampleUtils::rotatePoseMatrix(-90.0f, 1.0f, 0, 0,
                                      &modelViewMatrix.data[0]);
   
        SampleUtils::scalePoseMatrix(kBowlScaleX, kBowlScaleY, kBowlScaleZ,
                                     &modelViewMatrix.data[0]);
        SampleUtils::multiplyMatrix(&projectionMatrix.data[0],
                                    &modelViewMatrix.data[0],
                                    &modelViewProjection.data[0]);

        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &objectVertices[0]);
        glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &objectNormals[0]);
        glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &objectTexCoords[0]);
        
        glBindTexture(GL_TEXTURE_2D, textures[1]->mTextureID);
        glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );
        glDrawElements(GL_TRIANGLES, NUM_OBJECT_INDEX, GL_UNSIGNED_SHORT,
                       (const GLvoid*) &objectIndices[0]);

        SampleUtils::checkGlError("MultiTargets renderFrame");

    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glDisableVertexAttribArray(vertexHandle);
    glDisableVertexAttribArray(normalHandle);
    glDisableVertexAttribArray(textureCoordHandle);

    QCAR::Renderer::getInstance().end();
}


void
configureVideoBackground()
{
    // Get the default video mode:
    QCAR::CameraDevice& cameraDevice = QCAR::CameraDevice::getInstance();
    QCAR::VideoMode videoMode = cameraDevice.
                                getVideoMode(QCAR::CameraDevice::MODE_DEFAULT);

    // Configure the video background
    QCAR::VideoBackgroundConfig config;
    config.mEnabled = true;
    config.mSynchronous = true;
    config.mPosition.data[0] = 0.0f;
    config.mPosition.data[1] = 0.0f;
    
    if (isActivityInPortraitMode)
    {
        //LOG("configureVideoBackground PORTRAIT");
        config.mSize.data[0] = videoMode.mHeight
                                * (screenHeight / (float)videoMode.mWidth);
        config.mSize.data[1] = screenHeight;
    }
    else
    {
        //LOG("configureVideoBackground LANDSCAPE");
        config.mSize.data[0] = screenWidth;
        config.mSize.data[1] = videoMode.mHeight
                            * (screenWidth / (float)videoMode.mWidth);
    }

    // Set the config:
    QCAR::Renderer::getInstance().setVideoBackgroundConfig(config);
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_initApplicationNative(
                            JNIEnv* env, jobject obj, jint width, jint height)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_initApplicationNative");
    
    // Store screen dimensions
    screenWidth = width;
    screenHeight = height;
        
    // Handle to the activity class:
    jclass activityClass = env->GetObjectClass(obj);

    jmethodID getTextureCountMethodID = env->GetMethodID(activityClass,
                                                    "getTextureCount", "()I");
    if (getTextureCountMethodID == 0)
    {
        LOG("Function getTextureCount() not found.");
        return;
    }

    textureCount = env->CallIntMethod(obj, getTextureCountMethodID);    
    if (!textureCount)
    {
        LOG("getTextureCount() returned zero.");
        return;
    }

    textures = new Texture*[textureCount];

    jmethodID getTextureMethodID = env->GetMethodID(activityClass,
        "getTexture", "(I)Lcom/qualcomm/QCARSamples/MultiTargets/Texture;");

    if (getTextureMethodID == 0)
    {
        LOG("Function getTexture() not found.");
        return;
    }

    // Register the textures
    for (int i = 0; i < textureCount; ++i)
    {

        jobject textureObject = env->CallObjectMethod(obj, getTextureMethodID, i); 
        if (textureObject == NULL)
        {
            LOG("GetTexture() returned zero pointer");
            return;
        }

        textures[i] = Texture::create(env, textureObject);
    }
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_deinitApplicationNative(
                                                        JNIEnv* env, jobject obj)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_deinitApplicationNative");

    // Release texture resources
    if (textures != 0)
    {    
        for (int i = 0; i < textureCount; ++i)
        {
            delete textures[i];
            textures[i] = NULL;
        }
    
        delete[]textures;
        textures = NULL;
        
        textureCount = 0;
    }
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_startCamera(JNIEnv *,
                                                                         jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_startCamera");

    // Initialize the camera:
    if (!QCAR::CameraDevice::getInstance().init())
        return;

    // Configure the video background
    configureVideoBackground();

    // Select the default mode:
    if (!QCAR::CameraDevice::getInstance().selectVideoMode(
                                QCAR::CameraDevice::MODE_DEFAULT))
        return;

    // Start the camera:
    if (!QCAR::CameraDevice::getInstance().start())
        return;

    // Start the tracker:
    QCAR::Tracker::getInstance().start();
    
    // Cache the projection matrix:
    const QCAR::Tracker& tracker = QCAR::Tracker::getInstance();
    const QCAR::CameraCalibration& cameraCalibration =
                                    tracker.getCameraCalibration();
    projectionMatrix = QCAR::Tool::getProjectionGL(cameraCalibration, 3.0f,
                                            1500.0f);
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_stopCamera(JNIEnv *,
                                                                   jobject)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_stopCamera");

    QCAR::Tracker::getInstance().stop();

    QCAR::CameraDevice::getInstance().stop();
    QCAR::CameraDevice::getInstance().deinit();
}

JNIEXPORT jboolean JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_toggleFlash(JNIEnv*, jobject, jboolean flash)
{
    return QCAR::CameraDevice::getInstance().setFlashTorchMode((flash==JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_autofocus(JNIEnv*, jobject)
{
    return QCAR::CameraDevice::getInstance().startAutoFocus()?JNI_TRUE:JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargets_setFocusMode(JNIEnv*, jobject, jint mode)
{
    return QCAR::CameraDevice::getInstance().setFocusMode(mode)?JNI_TRUE:JNI_FALSE;
}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargetsRenderer_initRendering(
                                                    JNIEnv* env, jobject obj)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargetsRenderer_initRendering");

    // Define clear color
    glClearColor(0.0f, 0.0f, 0.0f, QCAR::requiresAlpha() ? 0.0f : 1.0f);
    
    // Now generate the OpenGL texture objects and add settings
    for (int i = 0; i < textureCount; ++i)
    {
        glGenTextures(1, &(textures[i]->mTextureID));
        glBindTexture(GL_TEXTURE_2D, textures[i]->mTextureID);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textures[i]->mWidth,
                textures[i]->mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                (GLvoid*)  textures[i]->mData);
    }
  
    shaderProgramID     = SampleUtils::createProgramFromBuffer(cubeMeshVertexShader,
                                                            cubeFragmentShader);

    vertexHandle        = glGetAttribLocation(shaderProgramID,
                                                "vertexPosition");
    normalHandle        = glGetAttribLocation(shaderProgramID,
                                                "vertexNormal");
    textureCoordHandle  = glGetAttribLocation(shaderProgramID,
                                                "vertexTexCoord");
    mvpMatrixHandle     = glGetUniformLocation(shaderProgramID,
                                                "modelViewProjectionMatrix");

}


JNIEXPORT void JNICALL
Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargetsRenderer_updateRendering(
                        JNIEnv* env, jobject obj, jint width, jint height)
{
    LOG("Java_com_qualcomm_QCARSamples_MultiTargets_MultiTargetsRenderer_updateRendering");
    
    // Update screen dimensions
    screenWidth = width;
    screenHeight = height;

    // Reconfigure the video background
    configureVideoBackground();
}


QCAR::ImageTarget*
findImageTarget(const char* name)
{
    QCAR::Tracker& tracker = QCAR::Tracker::getInstance();

    for(int i=0; i<tracker.getNumTrackables(); i++)
    {
        if(tracker.getTrackable(i)->getType()==QCAR::Trackable::IMAGE_TARGET)
        {
            if(!strcmp(tracker.getTrackable(i)->getName(),name))
                return reinterpret_cast<QCAR::ImageTarget*>(tracker.getTrackable(i));
        }
    }

    return NULL;
}


void
initMIT()
{
    //
    // This function checks the current tracking setup for completeness. If
    // it finds that something is missing, then it creates it and configures it:
    // Any MultiTarget and Part elements missing from the config.xml file
    // will be created.
    //

    LOG("Beginning to check the tracking setup");

    // Configuration data - identical to what is in the config.xml file
    //
    // If you want to recreate the trackable assets using the on-line TMS server 
    // using the original images provided in the sample's media folder, use the
    // following trackable sizes on creation to get identical visual results:
    // create a cuboid with width = 90 ; height = 120 ; length = 60.
    
    const char* names[6]   = { "FlakesBox.Front", "FlakesBox.Back", "FlakesBox.Left", "FlakesBox.Right", "FlakesBox.Top", "FlakesBox.Bottom" };
    const float trans[3*6] = { 0.0f,  0.0f,  30.0f, 
                               0.0f,  0.0f, -30.0f,
                              -45.0f, 0.0f,  0.0f, 
                               45.0f, 0.0f,  0.0f,
                               0.0f,  60.0f, 0.0f,
                               0.0f, -60.0f, 0.0f };
    const float rots[4*6]  = { 1.0f, 0.0f, 0.0f,   0.0f,
                               0.0f, 1.0f, 0.0f, 180.0f,
                               0.0f, 1.0f, 0.0f, -90.0f,
                               0.0f, 1.0f, 0.0f,  90.0f,
                               1.0f, 0.0f, 0.0f, -90.0f,
                               1.0f, 0.0f, 0.0f,  90.0f };

    QCAR::Tracker& tracker = QCAR::Tracker::getInstance();

    // Go through all Trackables to find the MultiTarget instance
    //
    for(int i=0; i<tracker.getNumTrackables(); i++)
    {
        if(tracker.getTrackable(i)->getType()==QCAR::Trackable::MULTI_TARGET)
        {
            LOG("MultiTarget exists -> no need to create one");
            mit = reinterpret_cast<QCAR::MultiTarget*>(tracker.getTrackable(i));
            break;
        }
    }

    // If no MultiTarget was found, then let's create one. The created
    // MultiTarget is automatically registered at the Tracker. No need to
    // keep a reference or destroy it manually later.
    //
    if(mit==NULL)
    {
        LOG("No MultiTarget found -> creating one");
        mit = QCAR::MultiTarget::create("FlakesBox");

        if(mit==NULL)
        {
            LOG("ERROR: Failed to create the MultiTarget - probably the Tracker is running");
            return;
        }
    }

    // Try to find each ImageTarget. If we find it, this actually means that it
    // is not part of the MultiTarget yet: ImageTargets that are part of a
    // MultiTarget don't show up in the list of Trackables.
    // Each ImageTarget that we found, is then made a part of the
    // MultiTarget and a correct pose (reflecting the pose of the
    // config.xml file) is set).
    // 
    int numAdded = 0;
    for(int i=0; i<6; i++)
    {
        if(QCAR::ImageTarget* it = findImageTarget(names[i]))
        {
            LOG("ImageTarget '%s' found -> adding it as to the MultiTarget",
                names[i]);

            int idx = mit->addPart(it);
            QCAR::Vec3F t(trans+i*3),a(rots+i*4);
            QCAR::Matrix34F mat;

            QCAR::Tool::setTranslation(mat, t);
            QCAR::Tool::setRotation(mat, a, rots[i*4+3]);
            mit->setPartOffset(idx, mat);
            numAdded++;
        }
    }

    LOG("Added %d ImageTarget(s) to the MultiTarget", numAdded);

    if(mit->getNumParts()!=6)
    {
        LOG("ERROR: The MultiTarget should have 6 parts, but it reports %d parts",
            mit->getNumParts());
    }

    LOG("Finished checking the tracking setup");
}


double
getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t = tv.tv_sec + tv.tv_usec/1000000.0;
    return t;
}


void
animateBowl(QCAR::Matrix44F& modelViewMatrix)
{
    static float rotateBowlAngle = 0.0f;

    static double prevTime = getCurrentTime();
    double time = getCurrentTime();             // Get real time difference
    float dt = (float)(time-prevTime);          // from frame to frame

    rotateBowlAngle += dt * 180.0f/3.1415f;     // Animate angle based on time

    SampleUtils::rotatePoseMatrix(rotateBowlAngle, 0.0f, 1.0f, 0.0f,
                                  &modelViewMatrix.data[0]);

    prevTime = time;
}


#ifdef __cplusplus
}
#endif
