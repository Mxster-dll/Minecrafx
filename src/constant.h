constexpr int    SCREEN_WIDTH = 800;
constexpr int    SCREEN_HEIGHT = 600;
constexpr double MOVE_SPEED = 0.1;           // 键盘移动速度（单位/帧）
constexpr double MOUSE_SENSITIVITY = 0.003;  // 鼠标视角灵敏度（弧度/像素）
constexpr double SLICE_STEP = 0.03;          // 切片旋转每格步长（弧度），滚轮/Q/E 统一
constexpr double SLICE_SMOOTH = 0.28;        // 速度平滑系数（0~1，越大越快）