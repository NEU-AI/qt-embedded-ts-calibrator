# 基于Qt4和tslib的触摸屏校准

## 编译
依赖 tslib.h 
```
qmake qt-ts-calibrator.pro
make
```

## 运行
启动程序，依次点击界面上的十字准星，五点后显示测试界面。
测试界面上九个按钮，点击每个按钮都可以吸住，说明校准成功。

## 集成
复制 calibration.cpp 和 calibration.h 到项目，
代码中加入：
```
#include "calibration.h"
// 以下代码启动校准窗口
Calibrator cal;
cal.exec();
```

