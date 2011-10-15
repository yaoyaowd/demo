#include "TargetDemo.h"

void displayMessage(char* message)
{
    jstring js = javaEnv->NewStringUTF(message);
    jmethodID method = javaEnv->GetMethodID(javaClass, "displayMessage", "(Ljava/lang/String;)V");
    javaEnv->CallVoidMethod(javaObj, method, js);
}

/* Set up all the buttons.
   1. Delete all the buttons added before.
   2. Add button if possible.*/
void VirtualButton_UpdateCallback::QCAR_onUpdate(QCAR::State& state)
{
	if (shouldUpdate)
	{
        QCAR::Tracker& tracker = QCAR::Tracker::getInstance();
        shouldAddButton = true;
        enabledButton = true;
		for (int i=0; i<tracker.getNumTrackables(); ++i)
		{
			QCAR::Trackable* trackable = tracker.getTrackable(i);
			QCAR::ImageTarget* target = static_cast<QCAR::ImageTarget*> (trackable);
            for (int j=0; j<maxb; j++)
            {
                QCAR::VirtualButton* button = target->getVirtualButton(buttonName[j]);
			    if (shouldAddButton && button == NULL)
			    {
				    QCAR::Rectangle vbRectangle(0, 0, 0, 0);
				    button = target->createVirtualButton(buttonName[j], vbRectangle);
			    }
			    if (button != NULL)
			    {
				    float left = cLeft[j], right = cLeft[j] + 2 * kButtonXYScale;
                    float bottom = cBottom[j] , top = cBottom[j] + 2 * kButtonXYScale;
				    QCAR::Rectangle vbRectangle(left, top, right, bottom);
				    button->setArea(vbRectangle);
				    button->setEnabled(true);
			    }
            }
		}
		shouldAddButton = false;
		shouldUpdate = false;
	}
}

void startJob(int posterId, int jobId)
{
    enabledButton = false;
    char c[200];
    if (jobId == 0)
    {
        strcpy(c, "1:");
        strcat(c, names[posterId]);
        strcat(c, ":");
        strcat(c, infos[posterId]);
    }
    else if (jobId == 1)
    {
        strcpy(c, "2:");
        strcat(c, infos[posterId]);
    }
    else if (jobId == 2)
    {
        strcpy(c, "3:");
        strcat(c, names[posterId]);
    }
    else if (jobId == 3)
    {
        strcpy(c, "4:");
        strcat(c, youtubes[posterId]);
    }
    displayMessage(c);
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_ImageTargetsRenderer_renderFrame(JNIEnv *, jobject)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
    QCAR::State state = QCAR::Renderer::getInstance().begin();
    for (int i=0; i<maxn; i++) tracked[i] = false;
    for(int tIdx = 0; tIdx < state.getNumActiveTrackables(); tIdx++)
    {
        shouldUpdate = true;
        const QCAR::Trackable* trackable = state.getActiveTrackable(tIdx);
        int id = getIndex(trackable->getName());
        const Texture* const thisTexture = textures[0];
        const QCAR::ImageTarget* target = static_cast<const QCAR::ImageTarget*> (trackable);

        for (int j=0; j<maxb; j++)
        {
            const QCAR::VirtualButton* button = target->getVirtualButton(buttonName[j]);
            if (button != NULL) {
                if (button->isPressed() && enabledButton2[id][j]) {
                    enabledButton2[id][j] = false;
                    startJob(id, j);
                }
            }
        }
	    tracked[id] = true;
	    modelViewMatrix[id] = QCAR::Tool::convertPose2GLMatrix(trackable->getPose());
	    shouldHandleTouch = true;
        renderAugmentation(trackable, id);
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

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_nativeTouchEvent(JNIEnv* , jobject, jfloat x, jfloat y)
{
    if (shouldHandleTouch)
	{
		float xFinal[maxn], yFinal[maxn];
		for (int id = 0; id<maxn; id++)
			if (tracked[id])
			{
				QCAR::Vec3F intersection, lineStart, lineEnd;
				projectScreenPointToPlane(QCAR::Vec2F(x, y), QCAR::Vec3F(0, 0, 0), QCAR::Vec3F(0, 0, 1), intersection, lineStart, lineEnd, id);
				xFinal[id] = intersection.data[0];
				yFinal[id] = intersection.data[1];
                for (int bId = 0; bId < maxb; bId++)
                    if (xFinal[id] >= cLeft[bId] && xFinal[id] <= cLeft[bId] + 2 * kButtonXYScale &&
                        yFinal[id] >= cBottom[bId] && yFinal[id] <= cBottom[bId] + 2 * kButtonXYScale)
                        startJob(id, bId);
			}
	}
}

void renderCube(float* transform, int k)
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
    SampleUtils::multiplyMatrix(&modelViewMatrix[k].data[0], transform, &objectMatrix.data[0]);
    SampleUtils::multiplyMatrix(&projectionMatrix.data[0], &objectMatrix.data[0], &modelViewProjection.data[0]);
    glUniformMatrix4fv(mvpMatrixHandle, 1, GL_FALSE, (GLfloat*)&modelViewProjection.data[0]);
    glDrawElements(GL_TRIANGLES, NUM_CUBE_INDEX, GL_UNSIGNED_SHORT, (const GLvoid*) &cubeIndices[0]);
#endif
}

void renderAugmentation(const QCAR::Trackable* trackable, int k)
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
    glLoadMatrixf(modelViewMatrix[k].data);
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
    
    float image_width = ((QCAR::ImageTarget*)trackable)->getSize().data[0];
    float image_height = ((QCAR::ImageTarget*)trackable)->getSize().data[1];
    for(int i = 0; i < maxb; i++)
    {
        Texture* buttonTexture = textures[i];
        float deltaX = cLeft[i] + kButtonXYScale;
        float deltaY = cBottom[i] + kButtonXYScale;
        QCAR::Matrix44F transform = SampleMath::Matrix44FIdentity();
        float* transformPtr = &transform.data[0];
        
        // The following transformations happen in reverse order
        // We want to scale the domino, tip the domino (on its leading edge), pivot and then position the domino
        SampleUtils::translatePoseMatrix(deltaX, deltaY, 0.0f, transformPtr);
        SampleUtils::rotatePoseMatrix(0, 0, 0, 1, transformPtr);
        SampleUtils::translatePoseMatrix(kButtonXYScale, 0.0f, 0.0f, transformPtr);
        SampleUtils::rotatePoseMatrix(0, 0, 1, 0, transformPtr);
        SampleUtils::translatePoseMatrix(-kButtonXYScale, 0.0f, kButtonZScale, transformPtr);
        SampleUtils::scalePoseMatrix(kButtonXYScale, kButtonXYScale, kButtonZScale, transformPtr);
        
        //SampleUtils::translatePoseMatrix(deltaX, deltaY, 0.0f, transformPtr);
        //SampleUtils::scalePoseMatrix(kGlowTextureScale, kGlowTextureScale, 0.0f, transformPtr);
        glBindTexture(GL_TEXTURE_2D, buttonTexture->mTextureID);
        renderCube(transformPtr, k);
    }
       
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

//-------------------------------------------------------------------------------------------------------------------

int getIndex(const char* name)
{
    for (int i=0; i<maxp; i++)
        if (strcmp(name, ids[i]) == 0)
            return i;
}

bool linePlaneIntersection(QCAR::Vec3F lineStart, QCAR::Vec3F lineEnd, QCAR::Vec3F pointOnPlane, QCAR::Vec3F planeNormal, QCAR::Vec3F &intersection)
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

void projectScreenPointToPlane(QCAR::Vec2F point, QCAR::Vec3F planeCenter, QCAR::Vec3F planeNormal, QCAR::Vec3F &intersection, QCAR::Vec3F &lineStart, QCAR::Vec3F &lineEnd, int k)
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
    QCAR::Matrix44F inverseModelViewMatrix = SampleMath::Matrix44FInverse(modelViewMatrix[k]);
    
    QCAR::Vec4F nearWorld = SampleMath::Vec4FTransform(pointOnNearPlane, inverseModelViewMatrix);
    QCAR::Vec4F farWorld = SampleMath::Vec4FTransform(pointOnFarPlane, inverseModelViewMatrix);
    
    lineStart = QCAR::Vec3F(nearWorld.data[0], nearWorld.data[1], nearWorld.data[2]);
    lineEnd = QCAR::Vec3F(farWorld.data[0], farWorld.data[1], farWorld.data[2]);
    linePlaneIntersection(lineStart, lineEnd, planeCenter, planeNormal, intersection);
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_deinitApplicationNative(JNIEnv* env, jobject obj)
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

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_ImageTargetsRenderer_initRendering(JNIEnv* env, jobject obj)
{
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

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_ImageTargetsRenderer_updateRendering(JNIEnv* env, jobject obj, jint width, jint height)
{
    // Update screen dimensions
    screenWidth = width;
    screenHeight = height;
    // Reconfigure the video background
    configureVideoBackground();
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_ImageTargetsRenderer_initNativeCallback(JNIEnv* env, jobject obj)
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

JNIEXPORT int JNICALL Java_com_demo_TargetDemo_TargetDemo_getOpenGlEsVersionNative(JNIEnv *, jobject)
{
#ifdef USE_OPENGL_ES_1_1        
    return 1;
#else
    return 2;
#endif
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_setActivityPortraitMode(JNIEnv *, jobject, jboolean isPortrait)
{
    isActivityInPortraitMode = isPortrait;
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_onQCARInitializedNative(JNIEnv *, jobject)
{
    // Comment in to enable tracking of up to 2 targets simultaneously and
    // split the work over multiple frames:
    QCAR::setHint(QCAR::HINT_MAX_SIMULTANEOUS_IMAGE_TARGETS, maxn);
    QCAR::setHint(QCAR::HINT_IMAGE_TARGET_MULTI_FRAME_ENABLED, 1);
    shouldUpdate = true;
    shouldAddButton = true;
    shouldHandleTouch = false;
    for (int i=0; i<10; i++) for (int j=0; j<10; j++) enabledButton2[i][j] = true;
    QCAR::registerCallback(&qcarUpdate);
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_startCamera(JNIEnv *, jobject)
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

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_stopCamera(JNIEnv *, jobject)
{
    //LOG("Java_com_demo_TargetDemo_TargetDemo_stopCamera");
    QCAR::Tracker::getInstance().stop();
    QCAR::CameraDevice::getInstance().stop();
    QCAR::CameraDevice::getInstance().deinit();
}

JNIEXPORT jboolean JNICALL Java_com_demo_TargetDemo_TargetDemo_toggleFlash(JNIEnv*, jobject, jboolean flash)
{
    return QCAR::CameraDevice::getInstance().setFlashTorchMode((flash==JNI_TRUE)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_demo_TargetDemo_TargetDemo_autofocus(JNIEnv*, jobject)
{
    return QCAR::CameraDevice::getInstance().startAutoFocus()?JNI_TRUE:JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_demo_TargetDemo_TargetDemo_setFocusMode(JNIEnv*, jobject, jint mode)
{
    return QCAR::CameraDevice::getInstance().setFocusMode(mode)?JNI_TRUE:JNI_FALSE;
}

void configureVideoBackground()
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
        config.mSize.data[0] = videoMode.mHeight * (screenHeight / (float)videoMode.mWidth);
        config.mSize.data[1] = screenHeight;
    }
    else
    {
        //LOG("configureVideoBackground LANDSCAPE");
        config.mSize.data[0] = screenWidth;
        config.mSize.data[1] = videoMode.mHeight * (screenWidth / (float)videoMode.mWidth);
    }
    // Set the config:
    QCAR::Renderer::getInstance().setVideoBackgroundConfig(config);
}

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_TargetDemo_initApplicationNative(JNIEnv* env, jobject obj, jint width, jint height)
{
    // Store screen dimensions
    screenWidth = width;
    screenHeight = height;
    // Handle to the activity class:
    jclass activityClass = env->GetObjectClass(obj);

    jmethodID getTextureCountMethodID = env->GetMethodID(activityClass, "getTextureCount", "()I");
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
    jmethodID getTextureMethodID = env->GetMethodID(activityClass, "getTexture", "(I)Lcom/demo/TargetDemo/Texture;");
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

JNIEXPORT void JNICALL Java_com_demo_TargetDemo_GUIManager_nativeRefresh(JNIEnv*, jobject)
{
    for (int i=0; i<10; i++)
        for (int j=0; j<10; j++)
            enabledButton2[i][j] = true;
    shouldUpdate = true;
}