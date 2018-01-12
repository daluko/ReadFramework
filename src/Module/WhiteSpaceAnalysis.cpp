/*******************************************************************************************************
 ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ. 
  
 Copyright (C) 2016 Markus Diem <diem@caa.tuwien.ac.at>
 Copyright (C) 2016 Stefan Fiel <fiel@caa.tuwien.ac.at>
 Copyright (C) 2016 Florian Kleber <kleber@caa.tuwien.ac.at>

 This file is part of ReadFramework.

 ReadFramework is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 ReadFramework is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 The READ project  has  received  funding  from  the European  Union’s  Horizon  2020  
 research  and innovation programme under grant agreement No 674943
 
 related links:
 [1] http://www.caa.tuwien.ac.at/cvl/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] http://nomacs.org
 *******************************************************************************************************/

#include "WhiteSpaceAnalysis.h"

#include "Image.h"
#include "Drawer.h"
#include "SuperPixelScaleSpace.h"
#include "SuperPixelClassification.h"
#include "ScaleFactory.h"

#include <opencv2/highgui/highgui.hpp>

#pragma warning(push, 0)	// no warnings from includes
#include <QSettings>
#include <QDebug>
#include <QPainter>
#include <QFileInfo>
#pragma warning(pop)

 // Qt defines

namespace rdf {

	// read defines

// WhiteSpaceConfig --------------------------------------------------------------------
	WhiteSpaceAnalysisConfig::WhiteSpaceAnalysisConfig() : ModuleConfig("White Space Analysis Module") {
	}

	QString WhiteSpaceAnalysisConfig::toString() const {
		return ModuleConfig::toString();
	}

	void WhiteSpaceAnalysisConfig::setShowResults(bool show) {
		mShowResults = show;
	}

	bool WhiteSpaceAnalysisConfig::ShowResults() const {
		return mShowResults;
	}

	void WhiteSpaceAnalysisConfig::setMinRectsPerSpace(int minRects) {
		mMinRectsPerSpace = minRects;
	}

	int WhiteSpaceAnalysisConfig::minRectsPerSpace() const {
		return ModuleConfig::checkParam(mMinRectsPerSpace, 0, INT_MAX, "minRectsPerSpace");
	}

	void WhiteSpaceAnalysisConfig::load(const QSettings & settings) {

		mShowResults = settings.value("ShowResults", ShowResults()).toBool();
		mMinRectsPerSpace = settings.value("minRectsPerSpace", minRectsPerSpace()).toInt();
	}

	void WhiteSpaceAnalysisConfig::save(QSettings & settings) const {

		settings.setValue("ShowResults", ShowResults());
		settings.setValue("minRectsPerSpace", minRectsPerSpace());
	}

// Module --------------------------------------------------------------------
WhiteSpaceAnalysis::WhiteSpaceAnalysis(const cv::Mat& img) {

	mImg = img;
	//ScaleFactory::instance().init(img.size());

	mConfig = QSharedPointer<WhiteSpaceAnalysisConfig>::create();
	mConfig->loadSettings();
}

bool WhiteSpaceAnalysis::isEmpty() const {
	return mImg.empty();
}

bool WhiteSpaceAnalysis::compute() {
	qDebug() << "computing white spaces...";

	if (!checkInput())
		return false;

	cv::Mat img = mImg;
	Timer dt;

	//TODO improve initial set of components used for text line formation 
	SuperPixel sp = computeSuperPixels(img);

	//TODO use additional filtering to reduce amount of noise components

	//convert super pixels (sp) to text components (tc)
	//save a list of final tc used for further processing
	mBlobs = sp.getMserBlobs();

	mBlobs;
	for (QSharedPointer<MserBlob> b : mBlobs) {
		QSharedPointer<MserBlob> rnn;
		for (QSharedPointer<MserBlob> nb : mBlobs) {
			if (b!=nb) {
				Rect r1(b->bbox().right(), b->bbox().top(), img.size().width, b->bbox().height());	
				
				Vector2D tr(img.size().width, b->bbox().height());
				Rect r(b->bbox().topRight(), tr);

				if (r.intersects(nb->bbox())) {
					if(rnn.isNull() || nb->bbox().left() < rnn->bbox().left())
						rnn = nb;
				}
			}
		}
	}

	//find right nearest neighbors (rnn) for each tc

	//compute white space rectangles (wsr)
	//refine list of wsr

	//find runs of wsr to determine white spaces (ws) between text blocks
	//refine list of wsr runs

	//form text blocks and text lines based on white spaces


	//if (img.rows > 2000) {
	//	// if you stumble upon this line:
	//	// microfilm images are binary with heavy noise
	//	// a small median filter fixes this issue...
	//	cv::medianBlur(img, img, 3);
	//}

	mInfo << "computed in" << dt;

	return true;
}

QSharedPointer<WhiteSpaceAnalysisConfig> WhiteSpaceAnalysis::config() const {
	return qSharedPointerDynamicCast<WhiteSpaceAnalysisConfig>(mConfig);
}

SuperPixel WhiteSpaceAnalysis::computeSuperPixels(const cv::Mat & img){

	// compute super pixels
	SuperPixel sp = SuperPixel(img);
	if (!sp.compute()) {
		qDebug() << "error during SuperPixel computation";
	}

	return sp;

	/**
	qInfo() << "super pixel computation time:" << dt;

	/////////////////////////
	//ouput grid super pixels
	// compute super pixels
	GridSuperPixel gsp(img);
	if (!gsp.compute()) {
		qDebug() << "error during SuperPixel computation";
	}

	mGsp = gsp.draw(img);
	qInfo() << "grid super pixel computation time:" << dt;

	////////////////////////////
	//ouput scale space super pixels
	// compute scale space super pixels

	qDebug() << "scale factor dpi: " << ScaleFactory::scaleFactorDpi();

	img = ScaleFactory::scaled(img);
	ScaleSpaceSuperPixel<GridSuperPixel> ssp(img);

	if (!ssp.compute()) {
		qDebug() << "error during SuperPixel computation";
	}

	mSsp = ssp.draw(img);
	qInfo() << "scale space super pixel computation time:" << dt;
	**/
}

cv::Mat WhiteSpaceAnalysis::draw(const cv::Mat & img, const QColor& col) const {

	// draw mser blobs
	Timer dtf;
	QImage qImg = Image::mat2QImage(img, true);
	QPainter p(&qImg);
	p.setPen(col);

	for (auto b : mBlobs) {

		if (!col.isValid())
			p.setPen(ColorManager::randColor());

		b->draw(p);
	}

	qDebug() << "drawing takes" << dtf;

	return Image::qImage2Mat(qImg);

	/**
	for (auto l : mStopLines) {
		l.setThickness(3);
		l.draw(p);
	}

	for (auto tb : mTextBlockSet.textBlocks()) {

		QPen pen(col);
		pen.setCosmetic(true);
		p.setPen(pen);

		QVector<PixelSet> s = tb->pixelSet().splitScales();
		for (int idx = s.size() - 1; idx >= 0; idx--) {

			if (!col.isValid())
				p.setPen(ColorManager::randColor());
			p.setOpacity(0.5);
			s[idx].draw(p, PixelSet::DrawFlags() | PixelSet::draw_pixels, Pixel::DrawFlags() | Pixel::draw_stats | Pixel::draw_ellipse);
			//qDebug() << "scale" << idx << ":" << *s[idx];
		}

		p.setOpacity(1.0);

		QPen tp(ColorManager::pink());
		tp.setWidth(5);
		p.setPen(tp);

		tb->draw(p, TextBlock::draw_text_lines);
	}
	**/
}

QString WhiteSpaceAnalysis::toString() const {
	return Module::toString();
}

bool WhiteSpaceAnalysis::checkInput() const {
	return !isEmpty();
}

// TextComponent --------------------------------------------------------------------
TextComponent::TextComponent(QSharedPointer<MserBlob> blob){
	mMserBlob = blob;
	mHasRnn = false;
}

void TextComponent::setRnn(TextComponent neighbor){
	mRnn = &neighbor;
	mHasRnn = true;
}

TextComponent* TextComponent::rnn() const{
	return mRnn;
}

QSharedPointer<MserBlob> TextComponent::mserBlob(){
	return mMserBlob;
}

bool TextComponent::hasRnn() const{
	return mHasRnn;
}

void TextComponent::draw(QPainter & p) const{
}

}