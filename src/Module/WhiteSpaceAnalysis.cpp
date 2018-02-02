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

#include <vector>
#include <algorithm>

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
	sp.pixelSet().pixels().at(1);

	//convert super pixels (sp) to text components (tc)
	mBlobs = sp.getMserBlobs();
	QVector<QSharedPointer<TextComponent>> tcM;

	for (QSharedPointer<MserBlob> b : mBlobs) {
		QSharedPointer<TextComponent> tc = QSharedPointer<TextComponent>(new TextComponent(b));
		tcM.append(tc);
	}

	//TODO use additional filtering to reduce amount of noise components
	//save a list of final tc used for further processing

	//sort tc according to y_max coordinate for more efficient computation
	QVector<QSharedPointer<TextComponent>> tcM_y;
	tcM_y = QVector<QSharedPointer<TextComponent>>(tcM);

	std::sort(tcM_y.begin(), tcM_y.end(), [](const auto& lhs, const auto& rhs){
		return lhs->mserBlob()->bbox().top() < rhs->mserBlob()->bbox().top();
	});

	//find right nearest neighbors (rnn) for each text component
	for (QSharedPointer<TextComponent> tc : tcM_y) {
		QSharedPointer<TextComponent> rnn;
		bool foundRnn = false;

		for (QSharedPointer<TextComponent> tcn : tcM_y) {

			QSharedPointer<MserBlob> tcBlob = tc->mserBlob();
			QSharedPointer<MserBlob> tcnBlob = tcn->mserBlob();

			if (tcBlob != tcnBlob) {
				
				Rect tcR = tcBlob->bbox();
				Rect tcnR = tcnBlob->bbox();

				//qDebug() << "tcnR is: " << tcnR.toString();
				Vector2D irSize(img.size().width - tcR.width(), tcR.height());
				Rect ir = Rect(tcR.topRight(), irSize);

				if (tcnR.center().x()>tcR.right() && ir.intersects(tcnR) ){	//center of neighbor should not be within components rect
					if(foundRnn){
						if (tcnR.left() < rnn->mserBlob()->bbox().left()) {
							rnn = tcn;
						}
					} else {
						rnn = tcn;
						foundRnn = true;
					}
				}
				else if(tcnR.top()>tcR.bottom()){
						break;
				}
			}
		}

		// if found  set rnn for current tc
		if (!rnn.isNull()) {
			//qDebug() << "tc is: " << tc->mserBlob()->bbox().toString();
			tc->setRnn(rnn);
			rnn->setLnn(tc);
			if (rnn->lnn().size() > 1) {
				rnn->setForkMarker(true);
			}
		}
	}

	//create initial text line candidates
	QVector<QSharedPointer<TextLineCandidate>> tlcM;
	for (auto tc : tcM_y) {
		if (!tc->hasLnn()){
			//create text line candidates
			QSharedPointer<TextLineCandidate> tlc;
			tlc = QSharedPointer<TextLineCandidate>(new TextLineCandidate(tc));
			tlcM.append(tlc);
		}
	}
	
	QVector<QSharedPointer<TextLineCandidate>> tlcM_final;
	for (auto tlc1 : tlcM) {
		bool final = true;
		for (auto tlc2 : tlcM) {
			if (tlc1 != tlc2) {
				if (tlc2->bbox().contains(tlc1->bbox())) {
					final = false;
					break;
				}
			}
		}
		if (final) {
			tlcM_final.append(tlc1);
		}
	}
	
	//compute white space rectangles (wsr)
	//refine list of wsr

	//find runs of wsr to determine white spaces (ws) between text blocks
	//refine list of wsr runs

	//form text blocks and text lines based on white spaces

	// draw debug image
	// create debug image for rnn
	QImage qImg = Image::mat2QImage(img, true);
	QPainter p(&qImg);

	for (QSharedPointer<TextLineCandidate> tlc : tlcM_final) {

		for (QSharedPointer<TextComponent> tc : tlc->TextComponents()) {
			
			//p.setPen(ColorManager::lightGray(1.0));
			//tc->mserBlob()->draw(p);

			if (!tc->hasRnn()) {
				//p.setPen(ColorManager::red(1.0));
				//tc->mserBlob()->draw(p);
			} 
			
			if (!tc->hasLnn()) {
				//p.setPen(ColorManager::blue(1.0));
				//tc->mserBlob()->draw(p);
			}
			
			if (tc->isAFork()) {
				p.setPen(ColorManager::darkGray(1.0));			
				tc->mserBlob()->draw(p);

				//for (auto b : tc->lnn()) {
				//	p.setPen(ColorManager::randColor());
				//	//b->mserBlob()->draw(p);
				//	b->drawLnns(p);
				//}
			}

			if (tc->hasRnn()) {
				p.setPen(ColorManager::red(1.0));
				QPoint point1 = tc->mserBlob()->center().toQPoint();
				QPoint point2 = tc->rnn()->mserBlob()->center().toQPoint();
				p.drawLine(point1, point2);
			}
		}

		p.setPen(ColorManager::blue(1.0));
		p.drawRect(tlc->bbox().toQRect());
	}

	mRnnImg = Image::qImage2Mat(qImg);
	//mRnnImg = sp.draw(img);
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
TextComponent::TextComponent() {
	mMserBlob = QSharedPointer<MserBlob>();
	mHasRnn = false;
	mHasLnn = false;
	mIsAFork = false;
}

TextComponent::TextComponent(QSharedPointer<MserBlob> blob){
	mMserBlob = blob;
	mHasRnn = false;
	mHasLnn = false;
	mIsAFork = false;
}

void TextComponent::setRnn(QSharedPointer<TextComponent> rn) {
	mRnn = rn;
	mHasRnn = true;
}

void TextComponent::setLnn(QSharedPointer<TextComponent> ln){
	mLnn.append(ln);
	mHasLnn = true;
}

void TextComponent::setForkMarker(bool isAFork){
	mIsAFork = isAFork;
}

QSharedPointer<TextComponent> TextComponent::rnn() const{
	return mRnn;
}

QVector<QSharedPointer<TextComponent>> TextComponent::lnn() const{
	return mLnn;
}

int TextComponent::bestFitLnnIdx() const {
	
	if (mHasLnn) {

		if (!mIsAFork) {
			return 0; //index of first (only) element
		}
		else {
			int maxRl = -1;
			int maxRlIdx = -1;
			for (int idx = 0; idx < mLnn.size(); ++idx) {
				int rl = mLnn.at(idx)->lnnRunLength();
				if (rl > maxRl) {
					maxRl = rl;
					maxRlIdx = idx;
				}
			}
			if (maxRl == -1) {
				qWarning() << "Couldn't find bestFitLnn Index!";
			}
			return maxRlIdx;
		}
	}
	else {
		return -1;
	}
}

int TextComponent::lnnRunLength() const{

	int lnnNum = 1;

	if (hasLnn()) {
		lnnNum += 1;
		
		int maxLnnRL = -1;
		for(auto tc:lnn()){
			int lnnRL = tc->lnnRunLength();
			if (lnnRL > maxLnnRL) {
				maxLnnRL = lnnRL;
			}
		}
		lnnNum += maxLnnRL;
	}

	return lnnNum;
}

void TextComponent::draw(const cv::Mat & img, const QColor& col) const {

	QImage qImg = Image::mat2QImage(img, true);

	QPainter p(&qImg);
	p.setPen(ColorManager::blue());
}

void TextComponent::drawLnns(QPainter &p){
	
	QColor col = p.pen().color();

	int lnnNum = 1;
	int maxLnnRL = -1;
	QSharedPointer<TextComponent> best_lnn;

	if (mHasLnn) {
		for (auto b : mLnn) {

			int lnnRL = b->lnnRunLength();
			if (lnnRL > maxLnnRL) {
				maxLnnRL = lnnRL;
				best_lnn = b;
			}
		}
		
		p.setPen(ColorManager::red(1.0));
		QPoint point1 = mserBlob()->center().toQPoint();
		QPoint point2 = best_lnn->mserBlob()->center().toQPoint();
		p.drawLine(point1, point2);

		col.setAlpha(60);
		p.setPen(col);

		best_lnn->mMserBlob->draw(p);
		best_lnn->drawLnns(p);

		lnnNum += maxLnnRL;
	}
}

QSharedPointer<MserBlob> TextComponent::mserBlob() const{
	return mMserBlob;
}

bool TextComponent::hasRnn() const{
	return mHasRnn;
}

bool TextComponent::hasLnn() const{
	return mHasLnn;
}

bool TextComponent::isAFork() const{
	return mIsAFork;
}

// TextLineCandidate --------------------------------------------------------------------

TextLineCandidate::TextLineCandidate(){
}

TextLineCandidate::TextLineCandidate(QSharedPointer<TextComponent> tc){
	mTextComponents.append(tc);

	if (tc->hasRnn()) {
		appendAllRnn(tc);
	}
}

void TextLineCandidate::merge(QSharedPointer<TextLineCandidate>){
}

int TextLineCandidate::length() const{
	return mTextComponents.size();
}

Rect TextLineCandidate::bbox() const {

	Rect bbox;

	for (QSharedPointer<TextComponent> tc : mTextComponents) {
		if (!bbox.isNull()) {
			bbox = bbox.joined(tc->mserBlob()->bbox());
		}
		else {
			bbox = tc->mserBlob()->bbox();
		}
	}

	return bbox;
}

QVector<QSharedPointer<TextComponent>> TextLineCandidate::TextComponents() const{
	return mTextComponents;
}

void TextLineCandidate::appendAllRnn(const QSharedPointer<TextComponent> tc){

	QSharedPointer<TextComponent> next = tc->rnn();

	//mTextComponents.append(next);
	//if (next->hasRnn()) {
	//	appendAllRnn(next);
	//}

	if(next->hasLnn()){
		if (!next->isAFork()) {
			mTextComponents.append(next);

			if (next->hasRnn()) {
				appendAllRnn(next);
			}
		}
		else if(next->isAFork()) {

			if (next->bestFitLnnIdx() != -1) {
				QSharedPointer<TextComponent> bestFitLnn = next->lnn().at(next->bestFitLnnIdx());
				if (tc->mserBlob() == bestFitLnn->mserBlob()) {
					mTextComponents.append(next);
					if (next->hasRnn()) {
						appendAllRnn(next);
					}
				}
			} else {
				qWarning() << "Found a fork without bestFitLnn!";
			}


			//int maxRl = -1;
			//QSharedPointer<TextComponent> maxTc;
			//for (auto ll : next->lnn()) {
			//	if (ll->lnnRunLength() > maxRl) {
			//		maxTc = ll;
			//	}
			//}

			//if (maxTc->mserBlob() == tc->mserBlob()) {	//continue appending rnns if tlc has longest lnn run on this fork
			//	mTextComponents.append(next);

			//	if (next->hasRnn()) {
			//		appendAllRnn(next);
			//	}
			//}
		}
	}	
	else {
		qWarning() << "Something went wrong! Found rnn text component without lnn.";
	}

}

}