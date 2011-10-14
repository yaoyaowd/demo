#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

#ifdef USE_OPENGL_ES_1_1
# include <GLES/gl.h>
# include <GLES/glext.h>
#else
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>
#endif

#include <QCAR/QCAR.h>
#include <QCAR/UpdateCallback.h>
#include <QCAR/CameraDevice.h>
#include <QCAR/Renderer.h>
#include <QCAR/VideoBackgroundConfig.h>
#include <QCAR/Trackable.h>
#include <QCAR/Tool.h>
#include <QCAR/Tracker.h>
#include <QCAR/CameraCalibration.h>
#include <QCAR/ImageTarget.h>
#include <QCAR/VirtualButton.h>
#include <QCAR/Rectangle.h>

#include "SampleMath.h"
#include "SampleUtils.h"
#include "Texture.h"
#include "CubeShaders.h"
#include "Cube.h"
//#include "Teapot.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Textures:
int textureCount                = 0;
Texture** textures              = 0;

// OpenGL ES 2.0 specific:
#ifdef USE_OPENGL_ES_2_0
unsigned int shaderProgramID    = 0;
GLint vertexHandle              = 0;
GLint normalHandle              = 0;
GLint textureCoordHandle        = 0;
GLint mvpMatrixHandle           = 0;
#endif

QCAR::Matrix44F inverseProjMatrix;
QCAR::Matrix44F projectionMatrix;
QCAR::Matrix44F modelViewMatrix;

// Screen dimensions:
unsigned int screenWidth        = 0;
unsigned int screenHeight       = 0;

// Indicates whether screen is in portrait (true) or landscape (false) mode
bool isActivityInPortraitMode   = false;

// Constants:
static const float kObjectScale = 3.f;
JNIEnv* javaEnv;
jobject javaObj;
jclass javaClass;

bool shouldAddButton;
bool shouldHandleTouch;
bool shouldEnableButton;

class VirtualButton_UpdateCallback : public QCAR::UpdateCallback {
    virtual void QCAR_onUpdate(QCAR::State& state);
} qcarUpdate;

void
displayMessage(char* message)
{
    // Use the environment and class stored in initNativeCallback
    // to call a Java method that displays a message via a toast
    jstring js = javaEnv->NewStringUTF(message);
    jmethodID method = javaEnv->GetMethodID(javaClass, "displayMessage", "(Ljava/lang/String;)V");
    javaEnv->CallVoidMethod(javaObj, method, js);
}

void VirtualButton_UpdateCallback::QCAR_onUpdate(QCAR::State& state)
{
    // Update runs in the tracking thread therefore it is guaranteed that the tracker is
    // not doing anything at this point. => Reconfiguration is possible.
    // Get the Tracker singleton
    QCAR::Tracker& tracker = QCAR::Tracker::getInstance();
    for (int i=0; i<tracker.getNumTrackables(); ++i)
    {
        QCAR::Trackable* trackable = tracker.getTrackable(i);
        QCAR::ImageTarget* target = static_cast<QCAR::ImageTarget*> (trackable);
        // Get the virtual button we created earlier, if it exists
        QCAR::VirtualButton* button = target->getVirtualButton("testButton");
        if (shouldAddButton && button == NULL)
        {
            // Create the virtual button
            QCAR::Rectangle vbRectangle(0, 0, 0, 0);
            button = target->createVirtualButton("testButton", vbRectangle);
        }
        if (shouldEnableButton && button != NULL)
        {
            float left = 0 - 30.f;
            float right = 0 + 30.f;
            float top = 0 + 30.f;
            float bottom = 0 - 30.f;
            QCAR::Rectangle vbRectangle(left, top, right, bottom);
            // Move the virtual button
            button->setArea(vbRectangle);
            // Enable the virtual button
            button->setEnabled(true);
        }
	if (!shouldEnableButton && button != NULL)
	{
	    button->setEnabled(false);
	    target->destroyVirtualButton(button);
	}
    }
    shouldAddButton = false;
    if (!shouldEnableButton)
    {
	    shouldAddButton = true;
	    shouldEnableButton = true;
    }
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_ImageTargetsRenderer_initNativeCallback(JNIEnv* env, jobject obj)
{
    // Store the java environment for later use
    // Note that this environment is only safe for use in this thread
    javaEnv = env;
    // Store the calling object for later use
    // Make a global reference to keep it valid beyond the scope of this function
    javaObj = env->NewGlobalRef(obj);
    // Store the class of the calling object for later use
    jclass objClass = env->GetObjectClass(obj);
    javaClass = (jclass) env->NewGlobalRef(objClass);
}

JNIEXPORT int JNICALL
Java_com_demo_TargetDemo_TargetDemo_getOpenGlEsVersionNative(JNIEnv *, jobject)
{
#ifdef USE_OPENGL_ES_1_1        
    return 1;
#else
    return 2;
#endif
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_TargetDemo_setActivityPortraitMode(JNIEnv *, jobject, jboolean isPortrait)
{
    isActivityInPortraitMode = isPortrait;
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_TargetDemo_onQCARInitializedNative(JNIEnv *, jobject)
{
    // Comment in to enable tracking of up to 2 targets simultaneously and
    // split the work over multiple frames:
    QCAR::setHint(QCAR::HINT_MAX_SIMULTANEOUS_IMAGE_TARGETS, 2);
    QCAR::setHint(QCAR::HINT_IMAGE_TARGET_MULTI_FRAME_ENABLED, 1);
    shouldAddButton = true;
    shouldEnableButton = true;
    shouldHandleTouch = false;
    QCAR::registerCallback(&qcarUpdate);
}

void renderCube(float* transform)
{
    // Render a cube with the given transform
    // Assumes prior GL setup
#ifdef USE_OPENGL_ES_1_1
    glPushMatrix();
    glMultMatrixf(transform);
    glDrawElements(GL_TRIANGLES, NUM_CUBE_INDEX, GL_UNSIGNED_SHORT, (const GLvoid*) &cubeIndices[0]);
    glPopMatrix();
#else
    QCAR::Matrix44F modelViewProjection, objectMatrix;
    SampleUtils::multiplyMatrix(&modelViewMatrix.data[0], transform, &objectMatrix.data[0]);
    SampleUtils::multiplyMatrix(&projectionMatrix.data[0], &objectMatrix.data[0], &modelViewProjection.data[0]);
    glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE, (GLfloat*)&modelViewProjection.data[0]);
    glDrawElements(GL_TRIANGLES, NUM_CUBE_INDEX, GL_UNSIGNED_SHORT, (const GLvoid*) &cubeIndices[0]);
#endif
}

static const float kGlowTextureScale    = 30.0f;
void renderAugmentation(const QCAR::Trackable* trackable)
{
    const Texture* const dominoTexture = textures[0];
#ifdef USE_OPENGL_ES_1_1
    // Set GL11 flags
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, (const GLvoid*) &cubeTexCoords[0]);
    glVertexPointer(3, GL_FLOAT, 0, (const GLvoid*) &cubeVertices[0]);
    glNormalPointer(GL_FLOAT, 0,  (const GLvoid*) &cubeNormals[0]);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    // Load projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projectionMatrix.data);
    // Load model view matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(modelViewMatrix.data);
#else
    // Bind shader program
    glUseProgram(shaderProgramID);
    // Set GL20 flags
    glEnableVertexAttribArray(vertexHandle);
    glEnableVertexAttribArray(normalHandle);
    glEnableVertexAttribArray(textureCoordHandle);
    glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*) &cubeVertices[0]);
    glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*) &cubeNormals[0]);
    glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*) &cubeTexCoords[0]);
#endif
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    // Render a green glow under the first domino, unless it is selected
    QCAR::Matrix44F transform = SampleMath::Matrix44FIdentity();
    float* transformPtr = &transform.data[0];
    SampleUtils::translatePoseMatrix(0.0, 0.0, 0.0f, transformPtr);
    SampleUtils::scalePoseMatrix(kGlowTextureScale, kGlowTextureScale, 0.0f, transformPtr);
    glBindTexture(GL_TEXTURE_2D, dominoTexture->mTextureID);
    renderCube(transformPtr);
    glDisable(GL_BLEND);
    // Render the dominoes
    glBindTexture(GL_TEXTURE_2D, dominoTexture->mTextureID);
    glDisable(GL_DEPTH_TEST);
#ifdef USE_OPENGL_ES_1_1        
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#else
    glDisableVertexAttribArray(vertexHandle);
    glDisableVertexAttribArray(normalHandle);
    glDisableVertexAttribArray(textureCoordHandle);
#endif
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_ImageTargetsRenderer_renderFrame(JNIEnv *, jobject)
{
    // Clear color and depth buffer 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Render video background:
    QCAR::State state = QCAR::Renderer::getInstance().begin();
#ifdef USE_OPENGL_ES_1_1
    // Set GL11 flags:
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
#endif
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    // Did we find any trackables this frame?
    for(int tIdx = 0; tIdx < state.getNumActiveTrackables(); tIdx++)
    {
        const QCAR::Trackable* trackable = state.getActiveTrackable(tIdx);
        // Choose the texture based on the target name:
        int textureIndex = 0;
        const Texture* const thisTexture = textures[textureIndex];
        if (strcmp(trackable->getName(), "expandables") == 0)
        {
            textureIndex = 0;
        }
        else if (strcmp(trackable->getName(), "fatezero") == 0)
        {
            textureIndex = 0;
        }
        const QCAR::ImageTarget* target = static_cast<const QCAR::ImageTarget*> (trackable);
        const QCAR::VirtualButton* button = target->getVirtualButton("testButton");
        if (button != NULL) {
            // If the virtual button is pressed, start the simulation
	   if (button->isPressed()) {
		displayMessage("touch button");
		shouldEnableButton = false;
            }
        }
	modelViewMatrix = QCAR::Tool::convertPose2GLMatrix(trackable->getPose());
	shouldHandleTouch = true;
        renderAugmentation(trackable);
/*
#ifdef USE_OPENGL_ES_1_1
        // Load projection matrix:
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(projectionMatrix.data);

        // Load model view matrix:
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(modelViewMatrix.data);
        glTranslatef(0.f, 0.f, kObjectScale);
        glScalef(kObjectScale, kObjectScale, kObjectScale);

        // Draw object:
        glBindTexture(GL_TEXTURE_2D, thisTexture->mTextureID);
        glTexCoordPointer(2, GL_FLOAT, 0, (const GLvoid*) &teapotTexCoords[0]);
        glVertexPointer(3, GL_FLOAT, 0, (const GLvoid*) &teapotVertices[0]);
        glNormalPointer(GL_FLOAT, 0,  (const GLvoid*) &teapotNormals[0]);
        glDrawElements(GL_TRIANGLES, NUM_TEAPOT_OBJECT_INDEX, GL_UNSIGNED_SHORT,
                       (const GLvoid*) &teapotIndices[0]);
#else
        QCAR::Matrix44F modelViewProjection;
        SampleUtils::translatePoseMatrix(0.0f, 0.0f, kObjectScale,
                                         &modelViewMatrix.data[0]);
        SampleUtils::scalePoseMatrix(kObjectScale, kObjectScale, kObjectScale,
                                     &modelViewMatrix.data[0]);
        SampleUtils::multiplyMatrix(&projectionMatrix.data[0],
                                    &modelViewMatrix.data[0] ,
                                    &modelViewProjection.data[0]);
        glUseProgram(shaderProgramID);
        glVertexAttribPointer(vertexHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &teapotVertices[0]);
        glVertexAttribPointer(normalHandle, 3, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &teapotNormals[0]);
        glVertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid*) &teapotTexCoords[0]);
        glEnableVertexAttribArray(vertexHandle);
        glEnableVertexAttribArray(normalHandle);
        glEnableVertexAttribArray(textureCoordHandle);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, thisTexture->mTextureID);
        glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE,
                           (GLfloat*)&modelViewProjection.data[0] );
        glDrawElements(GL_TRIANGLES, NUM_TEAPOT_OBJECT_INDEX, GL_UNSIGNED_SHORT,
                       (const GLvoid*) &teapotIndices[0]);
        SampleUtils::checkGlError("TargetDemo renderFrame");
#endif
*/
    }
    glDisable(GL_DEPTH_TEST);
#ifdef USE_OPENGL_ES_1_1        
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#else
    glDisableVertexAttribArray(vertexHandle);
    glDisableVertexAttribArray(normalHandle);
    glDisableVertexAttribArray(textureCoordHandle);
#endif
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
Java_com_demo_TargetDemo_TargetDemo_initApplicationNative(
                            JNIEnv* env, jobject obj, jint width, jint height)
{
    // Store screen dimensions
    screenWidth = width;
    screenHeight = height;
    // Handle to the activity class:
    jclass activityClass = env->GetObjectClass(obj);

    jmethodID getTextureCountMethodID = env->GetMethodID(activityClass,
                                                    "getTextureCount", "()I");
    if (getTextureCountMethodID == 0)
    {
        //LOG("Function getTextureCount() not found.");
        return;
    }
    textureCount = env->CallIntMethod(obj, getTextureCountMethodID);    
    if (!textureCount)
    {
        //LOG("getTextureCount() returned zero.");
        return;
    }
    textures = new Texture*[textureCount];
    jmethodID getTextureMethodID = env->GetMethodID(activityClass,
        "getTexture", "(I)Lcom/demo/TargetDemo/Texture;");
    if (getTextureMethodID == 0)
    {
        //LOG("Function getTexture() not found.");
        return;
    }
    // Register the textures
    for (int i = 0; i < textureCount; ++i)
    {

        jobject textureObject = env->CallObjectMethod(obj, getTextureMethodID, i); 
        if (textureObject == NULL)
        {
            //LOG("GetTexture() returned zero pointer");
            return;
        }
        textures[i] = Texture::create(env, textureObject);
    }
}


JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_TargetDemo_deinitApplicationNative(
                                                        JNIEnv* env, jobject obj)
{
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
Java_com_demo_TargetDemo_TargetDemo_startCamera(JNIEnv *, jobject)
{
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
    // Uncomment to enable flash
    //if(QCAR::CameraDevice::getInstance().setFlashTorchMode(true))
    //	LOG("IMAGE TARGETS : enabled torch");
    // Uncomment to enable infinity focus mode, or any other supported focus mode
    // See CameraDevice.h for supported focus modes
    //if(QCAR::CameraDevice::getInstance().setFocusMode(QCAR::CameraDevice::FOCUS_MODE_INFINITY))
    //	LOG("IMAGE TARGETS : enabled infinity focus");
    // Start the tracker:
    QCAR::Tracker::getInstance().start();
    // Cache the projection matrix:
    const QCAR::Tracker& tracker = QCAR::Tracker::getInstance();
    const QCAR::CameraCalibration& cameraCalibration =
                                    tracker.getCameraCalibration();
    projectionMatrix = QCAR::Tool::getProjectionGL(cameraCalibration, 2.0f,
                                            2000.0f);
    QCAR::Tracker::getInstance().start();
    inverseProjMatrix = SampleMath::Matrix44FInverse(projectionMatrix);
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_TargetDemo_stopCamera(JNIEnv *,
                                                                   jobject)
{
    //LOG("Java_com_demo_TargetDemo_TargetDemo_stopCamera");
    QCAR::Tracker::getInstance().stop();
    QCAR::CameraDevice::getInstance().stop();
    QCAR::CameraDevice::getInstance().deinit();
}

JNIEXPORT jboolean JNICALL
Java_com_demo_TargetDemo_TargetDemo_toggleFlash(JNIEnv*, jobject, jboolean flash)
{
    return QCAR::CameraDevice::getInstance().setFlashTorchMode((flash==JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_demo_TargetDemo_TargetDemo_autofocus(JNIEnv*, jobject)
{
    return QCAR::CameraDevice::getInstance().startAutoFocus()?JNI_TRUE:JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_demo_TargetDemo_TargetDemo_setFocusMode(JNIEnv*, jobject, jint mode)
{
    return QCAR::CameraDevice::getInstance().setFocusMode(mode)?JNI_TRUE:JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_ImageTargetsRenderer_initRendering(
                                                    JNIEnv* env, jobject obj)
{
    //LOG("Java_com_demo_TargetDemoRenderer_initRendering");

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
#ifndef USE_OPENGL_ES_1_1
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
#endif
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_ImageTargetsRenderer_updateRendering(
                        JNIEnv* env, jobject obj, jint width, jint height)
{
    // Update screen dimensions
    screenWidth = width;
    screenHeight = height;
    // Reconfigure the video background
    configureVideoBackground();
}

bool linePlaneIntersection(QCAR::Vec3F lineStart, QCAR::Vec3F lineEnd,
                      QCAR::Vec3F pointOnPlane, QCAR::Vec3F planeNormal,
                      QCAR::Vec3F &intersection)
{
    QCAR::Vec3F lineDir = SampleMath::Vec3FSub(lineEnd, lineStart);
    lineDir = SampleMath::Vec3FNormalize(lineDir);
    
    QCAR::Vec3F planeDir = SampleMath::Vec3FSub(pointOnPlane, lineStart);
    
    float n = SampleMath::Vec3FDot(planeNormal, planeDir);
    float d = SampleMath::Vec3FDot(planeNormal, lineDir);
    if (fabs(d) < 0.00001) {
        return false;
    }
    float dist = n / d;
    
    QCAR::Vec3F offset = SampleMath::Vec3FScale(lineDir, dist);
    intersection = SampleMath::Vec3FAdd(lineStart, offset);
}

void
projectScreenPointToPlane(QCAR::Vec2F point, QCAR::Vec3F planeCenter, QCAR::Vec3F planeNormal,
                          QCAR::Vec3F &intersection, QCAR::Vec3F &lineStart, QCAR::Vec3F &lineEnd)
{
    // Window Coordinates to Normalized Device Coordinates
    QCAR::VideoBackgroundConfig config = QCAR::Renderer::getInstance().getVideoBackgroundConfig();
    
    float halfScreenWidth = screenWidth / 2.0f;
    float halfScreenHeight = screenHeight / 2.0f;
    
    float halfViewportWidth = config.mSize.data[0] / 2.0f;
    float halfViewportHeight = config.mSize.data[1] / 2.0f;
    
    float x = (point.data[0] - halfScreenWidth) / halfViewportWidth;
    float y = (point.data[1] - halfScreenHeight) / halfViewportHeight * -1;
    
    QCAR::Vec4F ndcNear(x, y, -1, 1);
    QCAR::Vec4F ndcFar(x, y, 1, 1);
    
    // Normalized Device Coordinates to Eye Coordinates
    QCAR::Vec4F pointOnNearPlane = SampleMath::Vec4FTransform(ndcNear, inverseProjMatrix);
    QCAR::Vec4F pointOnFarPlane = SampleMath::Vec4FTransform(ndcFar, inverseProjMatrix);
    pointOnNearPlane = SampleMath::Vec3FDiv(pointOnNearPlane, pointOnNearPlane.data[3]);
    pointOnFarPlane = SampleMath::Vec3FDiv(pointOnFarPlane, pointOnFarPlane.data[3]);
    
    // Eye Coordinates to Object Coordinates
    QCAR::Matrix44F inverseModelViewMatrix = SampleMath::Matrix44FInverse(modelViewMatrix);
    
    QCAR::Vec4F nearWorld = SampleMath::Vec4FTransform(pointOnNearPlane, inverseModelViewMatrix);
    QCAR::Vec4F farWorld = SampleMath::Vec4FTransform(pointOnFarPlane, inverseModelViewMatrix);
    
    lineStart = QCAR::Vec3F(nearWorld.data[0], nearWorld.data[1], nearWorld.data[2]);
    lineEnd = QCAR::Vec3F(farWorld.data[0], farWorld.data[1], farWorld.data[2]);
    linePlaneIntersection(lineStart, lineEnd, planeCenter, planeNormal, intersection);
}

JNIEXPORT void JNICALL
Java_com_demo_TargetDemo_TargetDemo_nativeTouchEvent(JNIEnv* , jobject, jfloat x, jfloat y)
{
    if (shouldHandleTouch)
    {
	    QCAR::Vec3F intersection, lineStart, lineEnd;
	    projectScreenPointToPlane(QCAR::Vec2F(x, y), QCAR::Vec3F(0, 0, 0), QCAR::Vec3F(0, 0, 1), intersection, lineStart, lineEnd);
	    QCAR::Vec2F position(intersection.data[0], intersection.data[1]);
    }
}

#ifdef __cplusplus
}
#endif