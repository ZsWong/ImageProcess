#include "mat.h"
#include "function.h"
#include <fstream>
#include <iostream>
#include <cassert>
#include <memory>

namespace wong {

BmpMat::~BmpMat() {
    if(rgbData){
        //如果指向像素的uni指针有效，释放uni指针指向的空间

        rgbData.reset();
    }
}

BmpMat::BmpMat(int row, int col) : height(row), width(col), size(0), bytesOfRow(0),
    bmpHeader(0){
    //根据输入的行号和列号，创建一个空图像

    freshSize();

    initRgbData();

    freshBmpHeader();
}

BmpMat::BmpMat(const BmpMat& sourceMat): height(sourceMat.getHeight()), width(sourceMat.getWidth()),
    size(sourceMat.getSize()), bytesOfRow(sourceMat.getSize() / sourceMat.getHeight()),
    bmpHeader(0){
    //拷贝构造函数

    freshBmpHeader();

    //拷贝正常rgb像素值
    char *pixPtr = new char[size];
    std::uninitialized_copy(sourceMat.getRgbData(), sourceMat.getRgbData() + size, pixPtr);
    rgbData.reset(pixPtr);
}

BmpMat& BmpMat::operator=(const BmpMat &rMat) {
    //赋值构造函数

    height = rMat.getHeight(); width = rMat.getWidth();

    size = rMat.getSize();

    bytesOfRow = size / height;

    freshBmpHeader();

    //复制像素数据
    char *pixPtr = new char[size];
    std::uninitialized_copy(rMat.getRgbData(), rMat.getRgbData() + size, pixPtr);
    rgbData.reset(pixPtr);

    return *this;
}

BmpMat& BmpMat::operator=(BmpMat &&rMat) noexcept{
    //移动赋值运算符

    height = rMat.getHeight(); width = rMat.getWidth();

    size = rMat.getSize();

    bytesOfRow = size / height;

    bmpHeader = rMat.bmpHeader;

    //移动像素数据
    rgbData = std::move(rMat.rgbData);

    return *this;
}

void BmpMat::initRgbData() {
    //初始化空像素值，rgb像素值，灰度像素值

    char *rgbnew = new char[size];
    rgbData.reset(rgbnew);
}

void BmpMat::freshSize(){
    //更新size和bytesOfRow

    bytesOfRow = width * 3;    //预测的每行字节数
    if (bytesOfRow % 4 != 0) {
        //如果当前的列数无法满足每列的字节数是4的倍数，则更改每行的字节数
        bytesOfRow = (bytesOfRow / 4 + 1) * 4;
    }

    size = height * bytesOfRow;   //图片总字节数!!!(字节)
}

void BmpMat::freshBmpHeader() {
    //更新bmp文件头中的行号、列号、图像总字节数

    if(bmpHeader.empty()){
        //如果头信息为空,构造信息头

        bmpHeader.resize(54);

        size_t cur = 0;
        for (char &ch : bmpHeader) {
            //装载信息头

            ch = strToch(headerData.substr(cur, cur + 8));
            cur += 8;
        }
    }

    char *cur;
    cur = &bmpHeader[18];
    longToChArr(width, cur, 4);
    cur = &bmpHeader[22];
    longToChArr(height, cur, 4);
    cur = &bmpHeader[34];
    longToChArr(size, cur, 4);
}



int BmpMat::getHeight() const{
    //得到行数

    return height;
}

int BmpMat::getWidth() const{
    //得到列数

    return width;
}

int BmpMat::getSize() const{
    //得到图像大小，保证了每一行的字节数是4的倍数

    return size;
}

char* BmpMat::getRgbData() const{
    //返回指向图像像素数据的指针

    return rgbData.get();
}

bool BmpMat::isNull() const{
    //空图（头信息为空）返回true

    if(bmpHeader.empty()){
        return true;
    }
    return false;
}

void BmpMat::resize(int r, int c, InsertType type){
    //重新规定bmp图像的大小

    if(isNull()){
        //如果图像为空，通过赋值，建立一个新图像

       *this = BmpMat( r, c);
       return;
    }

    double rowK = static_cast<double>(r) / height, colK = static_cast<double>(c) / width;    //计算行列的缩放比列

    resize(rowK, colK, type);
}

void BmpMat::resize(double yk, double xk, InsertType type){
    //按照缩放因子重整大小

    assert(height > 0 && width > 0);

    int newHeight = static_cast<int>(std::round(static_cast<double>(height) * yk));
    int newWidth = static_cast<int>(std::round(static_cast<double>(width) * xk));

    BmpMat tempMat(newHeight, newWidth);

    std::array<char, 3> valCopy;
    for (int y = 1; y != newHeight + 1; ++y) {
        for (int x = 1.0; x != newWidth + 1; ++x) {
            valCopy = ptr(static_cast<double>(y) / yk, static_cast<double>(x) / xk, type);
            tempMat.at(y, x)[0] = valCopy[0];
            tempMat.at(y, x)[1] = valCopy[1];
            tempMat.at(y, x)[2] = valCopy[2];
        }
    }

    *this = std::move(tempMat);
}

void BmpMat::resize(double k, InsertType type){
    //行列按照同样的尺度缩放

    assert(height > 0 && width > 0);

    resize(k, k, type);
}

char* BmpMat::at(int y, int x) {
    //返回指向位置（y，x）的像素的指针

    assert(y >= 1 && y <= height && x >= 1 && x <= width);

    int cur = size - y * bytesOfRow + 3 * (x - 1);
    return rgbData.get() + cur;
}

std::array<char, 3> BmpMat::ptr(float y, float x, InsertType type) const{
    //返回位置（y，x）的像素的拷贝，插值方式为type

    //leftB-最左边x的值；rightB-最右边x的值；upB-最上面y的值；buttomB-最下面y的值
    int leftB = static_cast<int>(floor(x)), rightB = static_cast<int>(ceil(x)),
        upB = static_cast<int>(floor(y)), buttomB = static_cast<int>(ceil(y));

    if (type == InsertType::Neiborhood) {
        //邻域插值

        int neiY = (buttomB - y) < (y - upB) ? buttomB : upB;
        int neiX = (rightB - x) < (x - leftB) ? rightB : leftB;
        std::array<int, 3> temp = matVal(neiY, neiX);
        std::array<char, 3> result;
        result[0] = static_cast<char>(temp[0]);
        result[1] = static_cast<char>(temp[1]);
        result[2] = static_cast<char>(temp[2]);

        return result;
    }
    else if (type == InsertType::DoubleLine) {
        //双线性插值

        std::array<int, 3> leftUpVal, rightUpVal, rightButtomVal, leftButtomVal;
        leftUpVal = matVal(upB, leftB); rightUpVal = matVal(upB, rightB);
        rightButtomVal = matVal(buttomB, rightB); leftButtomVal = matVal(buttomB, leftB);

        std::array<int, 3> temp;
        std::array<char, 3> result;
        float valLeft, valRight;
        valLeft = leftButtomVal[0] + (buttomB - y) * (leftUpVal[0] - leftButtomVal[0]);
        valRight = rightButtomVal[0] + (buttomB - y) * (rightUpVal[0] - rightButtomVal[0]);
        temp[0] = static_cast<int>(valLeft + (x - leftB) * (valRight - valLeft));

        valLeft = leftButtomVal[1] + (buttomB - y) * (leftUpVal[1] - leftButtomVal[1]);
        valRight = rightButtomVal[1] + (buttomB - y) * (rightUpVal[1] - rightButtomVal[1]);

        temp[1] = static_cast<int>(valLeft + (x - leftB) * (valRight - valLeft));

        valLeft = leftButtomVal[2] + (buttomB - y) * (leftUpVal[2] - leftButtomVal[2]);
        valRight = rightButtomVal[2] + (buttomB - y) * (rightUpVal[2] - rightButtomVal[2]);
        temp[2] = static_cast<int>(valLeft + (x - leftB) * (valRight - valLeft));

        assert(temp[0] >= 0 && temp[1] >= 0 && temp[2] >= 0);
        result[0] = static_cast<char>(temp[0]);
        result[1] = static_cast<char>(temp[1]);
        result[2] = static_cast<char>(temp[2]);

        return result;
    }
    else{
        std::array<char, 3> tmpArr;
        tmpArr.fill(static_cast<char>(static_cast<char>(255)));
        return tmpArr;
    }
}

std::array<int, 3> BmpMat::matVal(int y, int x) const{
    //返回（y，x）处的像素值，如果（y，x）不存在，返回白色像素点

    std::array<int, 3> val = {255, 255, 255};

    int cur = size - y * bytesOfRow + (x - 1) * 3;
    int v;
    if ((y <= height) && (y >= 1) && (x <= width) && (x >= 1)) {
        v = rgbData[cur];
        val[0] = v < 0 ? v + 256 : v;
        v = rgbData[cur + 1];
        val[1] = v < 0 ? v + 256 : v;
        v = rgbData[cur + 2];
        val[2] = v < 0 ? v + 256 : v;
        return val;
    }
    else {
        return val;
    }
}

void wong::BmpMat::writeMat(const std::string fileName){
    //保存图片

    std::ofstream o;
    o.open(fileName, std::ofstream::binary);

    for (char ch : bmpHeader) {
        o.put(ch);
    }

    o.write(rgbData.get(), size);
    o.close();
}

}
