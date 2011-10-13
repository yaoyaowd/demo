/*============================================================================
            Copyright (c) 2010-2011 QUALCOMM Incorporated.
            All Rights Reserved.
            Qualcomm Confidential and Proprietary
  ============================================================================*/


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

#include <Cube.h>
#include <CubeShaders.h>
#include <SampleUtils.h>
#include <SampleMath.h>
#include <Texture.h>


#define MAX_DOMINOES 100
#define DOMINO_TILT_SPEED 300.0f
#define MAX_TAP_TIMER 200
#define MAX_TAP_DISTANCE2 400


enum ActionType {
    ACTION_DOWN,
    ACTION_MOVE,
    ACTION_UP,
    ACTION_CANCEL
};

enum DominoState {
    DOMINO_STANDING,
    DOMINO_FALLING,
    DOMINO_RESTING
};


typedef struct _TouchEvent {
    bool isActive;
    int actionType;
    int pointerId;
    float x;
    float y;
    float lastX;
    float lastY;
    float startX;
    float startY;
    float tapX;
    float tapY;
    unsigned long startTime;
    unsigned long dt;
    float dist2;
    bool didTap;
} TouchEvent;

typedef struct _LLNode {
    int id;
    _LLNode* next;
} LLNode;

typedef struct _Domino {
    int id;
    int state;
    
    LLNode* neighborList;
    
    QCAR::Vec2F position;
    float pivotAngle;
    float tiltAngle;
    QCAR::Matrix44F transform;
    QCAR::Matrix44F pickingTransform;
    
    int tippedBy;
    int restingFrameCount;
} Domino;

class VirtualButton_UpdateCallback : public QCAR::UpdateCallback {
    virtual void QCAR_onUpdate(QCAR::State& state);
} qcarUpdate;


JNIEnv* javaEnv;
jobject javaObj;
jclass javaClass;

bool displayedMessage;

unsigned long lastSystemTime;
unsigned long lastTapTime;

QCAR::Matrix44F modelViewMatrix;

TouchEvent touch1, touch2;
bool simulationRunning;
bool shouldResetDominoes;
bool shouldClearDominoes;
bool shouldDeleteSelectedDomino;

Domino dominoArray[MAX_DOMINOES];
int dominoCount;
int uniqueId;

int dropStartIndex;
QCAR::Vec2F lastDropPosition(0, 0);

Domino* selectedDomino;
int selectedDominoIndex;

QCAR::Vec3F dominoBaseVertices[8];
QCAR::Vec3F dominoTransformedVerticesA[8];
QCAR::Vec3F dominoTransformedVerticesB[8];
QCAR::Vec3F dominoNormals[3];

bool shouldUpdateButton;
bool shouldAddButton;
bool shouldMoveButton;
bool shouldEnableButton;
bool shouldDisableButton;
bool shouldRemoveButton;

Domino* vbDomino;


#ifdef __cplusplus
extern "C"
{
#endif
    
    // Cube texture:
    extern int textureCount;
    extern Texture** textures;
    
    #ifdef USE_OPENGL_ES_2_0
    // OpenGL ES 2.0 specific:
    extern unsigned int shaderProgramID;
    extern GLint vertexHandle;
    extern GLint normalHandle;
    extern GLint textureCoordHandle;
    extern GLint mvpMatrixHandle;
    #endif
    
    extern int screenWidth;
    extern int screenHeight;
    
    extern QCAR::Matrix44F projectionMatrix;
    extern QCAR::Matrix44F inverseProjMatrix;
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_Dominoes_onQCARInitializedNative(JNIEnv* env, jobject);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_DominoesRenderer_initNativeCallback(JNIEnv* env, jobject obj);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_DominoesRenderer_renderFrame(JNIEnv*, jobject);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_Dominoes_nativeTouchEvent(JNIEnv*, jobject, jint actionType, jint pointerId, jfloat x, jfloat y);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_GUIManager_nativeStart(JNIEnv*, jobject);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_GUIManager_nativeReset(JNIEnv*, jobject);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_GUIManager_nativeClear(JNIEnv*, jobject);
    
    JNIEXPORT void JNICALL
    Java_com_qualcomm_QCARSamples_Dominoes_GUIManager_nativeDelete(JNIEnv*, jobject);
    
#ifdef __cplusplus
}
#endif

void playSoundEffect(int soundIndex, float volume = 1.0f);
void showDeleteButton();
void hideDeleteButton();
void toggleStartButton();
void displayMessage(char* message);

void updateAugmentation(const QCAR::Trackable* trackable, float dt);
void handleTouches();
void renderAugmentation(const QCAR::Trackable* trackable);
void renderCube(float* transform);

void addVirtualButton();
void removeVirtualButton();
void moveVirtualButton(Domino* domino);
void enableVirtualButton();
void disableVirtualButton();

void initDominoBaseVertices();
void initDominoNormals();

bool canDropDomino(QCAR::Vec2F position);
void dropDomino(QCAR::Vec2F position);
void updateDominoTransform(Domino* domino);
void updatePickingTransform(Domino* domino);

void runSimulation(Domino* domino, float dt);
void handleCollision(Domino* domino, Domino* otherDomino, float originalTilt);
void adjustPivot(Domino* domino, Domino* otherDomino);
Domino* getDominoById(int id);

void resetDominoes();
void clearDominoes();
void setSelectedDomino(Domino* domino);
void deleteSelectedDomino();

void projectScreenPointToPlane(QCAR::Vec2F point, QCAR::Vec3F planeCenter, QCAR::Vec3F planeNormal,
                               QCAR::Vec3F &intersection, QCAR::Vec3F &lineStart, QCAR::Vec3F &lineEnd);
bool linePlaneIntersection(QCAR::Vec3F lineStart, QCAR::Vec3F lineEnd, QCAR::Vec3F pointOnPlane,
                           QCAR::Vec3F planeNormal, QCAR::Vec3F &intersection);

bool checkIntersection(QCAR::Matrix44F transformA, QCAR::Matrix44F transformB);
bool isSeparatingAxis(QCAR::Vec3F axis);

bool checkIntersectionLine(QCAR::Matrix44F transformA, QCAR::Vec3F pointA, QCAR::Vec3F pointB);
bool isSeparatingAxisLine(QCAR::Vec3F axis, QCAR::Vec3F pointA, QCAR::Vec3F pointB);

unsigned long getCurrentTimeMS();
