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
#include "TextHeightEstimation.h"
#include "NonTextFiltering.h"
#include "FontStyleClassification.h"
#include "SuperPixelClassification.h"
#include "ScaleFactory.h"
#include "GraphCut.h"
#include "ElementsHelper.h"
#include "LineTrace.h"

#include <vector>
#include <algorithm>

#pragma warning(push, 0)	// no warnings from includes
#include <QSettings>
#include <QDebug>
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#pragma warning(pop)

 // Qt defines

namespace rdf {

// WhiteSpaceConfig --------------------------------------------------------------------
	WhiteSpaceAnalysisConfig::WhiteSpaceAnalysisConfig() : ModuleConfig("White Space Analysis Module") {
	}

	QString WhiteSpaceAnalysisConfig::toString() const {
		return ModuleConfig::toString();
	}

	void WhiteSpaceAnalysisConfig::setNumErosionLayers(int numErosionLayers) {
		mNumErosionLayers = numErosionLayers;
	}

	int WhiteSpaceAnalysisConfig::numErosionLayers() const {
		return ModuleConfig::checkParam(mNumErosionLayers, 0, INT_MAX, "numErosionLayers");
	}

	void WhiteSpaceAnalysisConfig::setMaxImgSide(int maxImgSide) {
		mMaxImgSide = maxImgSide;
	}

	int WhiteSpaceAnalysisConfig::maxImgSide() const {
		return ModuleConfig::checkParam(mMaxImgSide, 0, INT_MAX, "maxImgSide");
	}

	void WhiteSpaceAnalysisConfig::setTextHeightMultiplier(double textHeightMultiplier){
		mTextHeightMultiplier = textHeightMultiplier;
	}

	double WhiteSpaceAnalysisConfig::textHeightMultiplier() const{
		return ModuleConfig::checkParam(mTextHeightMultiplier, 0.1, 5.0, "textHeightMultiplier");
	}

	void WhiteSpaceAnalysisConfig::setScaleInput(bool scaleInput) {
		mScaleInput = scaleInput;
	}

	bool WhiteSpaceAnalysisConfig::scaleInput() const {
		return mScaleInput;
	}

	void WhiteSpaceAnalysisConfig::setAthenaScaling(bool athenaScaling){
		mAthenaScaling = athenaScaling;
	}

	bool WhiteSpaceAnalysisConfig::athenaScaling() const{
		return mAthenaScaling;
	}

	void WhiteSpaceAnalysisConfig::setDebugDraw(bool debugDraw){
		mDebugDraw = debugDraw;
	}

	bool WhiteSpaceAnalysisConfig::debugDraw() const{
		return mDebugDraw;
	}

	void WhiteSpaceAnalysisConfig::setBlackSeparators(bool bs) {
		mBlackSeparators = bs;
	}

	bool WhiteSpaceAnalysisConfig::blackSeparators() const {
		return mBlackSeparators;
	}

	void WhiteSpaceAnalysisConfig::setDebugPath(const QString & dp) {
		mDebugPath = dp;
	}

	QString WhiteSpaceAnalysisConfig::debugPath() const {
		return mDebugPath;
	}
	
	void WhiteSpaceAnalysisConfig::load(const QSettings & settings) {

		mNumErosionLayers		= settings.value("numErosionLayers", numErosionLayers()).toInt();
		mMaxImgSide				= settings.value("maxImgSide", maxImgSide()).toInt();
		mTextHeightMultiplier	= settings.value("textHeightMultiplier", textHeightMultiplier()).toDouble();
		mScaleInput				= settings.value("scaleInput", scaleInput()).toBool();
		mAthenaScaling			= settings.value("athenaScaling", athenaScaling()).toBool();
		mDebugDraw				= settings.value("debugDraw", debugDraw()).toBool();
		mBlackSeparators		= settings.value("blackSeparators", blackSeparators()).toBool();
		mDebugPath				= settings.value("debugPath", debugPath()).toString();
	}

	void WhiteSpaceAnalysisConfig::save(QSettings & settings) const {

		settings.setValue("numErosionLayers", numErosionLayers());
		settings.setValue("maxImgSide", maxImgSide());
		settings.setValue("textHeightMultiplier", textHeightMultiplier());
		settings.setValue("scaleInput", scaleInput());
		settings.setValue("athenaScaling", athenaScaling());
		settings.setValue("debugDraw", debugDraw());
		settings.setValue("blackSeparators", blackSeparators());
		settings.setValue("debugPath", debugPath());
	}


// WhiteSpaceAnalysis --------------------------------------------------------------------
WhiteSpaceAnalysis::WhiteSpaceAnalysis(const cv::Mat& img) {
	mImg = img;
	mTextBlockRegions = QSharedPointer<Region>::create();

	mConfig = QSharedPointer<WhiteSpaceAnalysisConfig>::create();
	mConfig->loadSettings();

	mTlhConfig = QSharedPointer<TextLineHypothesizerConfig>::create();
	mTlhConfig->loadSettings();

	mWssConfig = QSharedPointer<WhiteSpaceSegmentationConfig>::create();
	mWssConfig->loadSettings();

	mTbfConfig = QSharedPointer<TextBlockFormationConfig>::create();
	mTbfConfig->loadSettings();

	mScaleFactory = QSharedPointer<ScaleFactory>(new ScaleFactory(img.size()));

	auto sfConfig = mScaleFactory->config();
	sfConfig->setMaxImageSide(config()->maxImgSide());
	mScaleFactory->setConfig(sfConfig);
}

bool WhiteSpaceAnalysis::isEmpty() const {
	return mImg.empty();
}

bool WhiteSpaceAnalysis::compute() {
	//TODO use asssert() function to check input parameters and results
	//TODO minor improvements:	merge horizontally aligned line fragments that are isolated or at top/bottom of text block
	//							try to simplify/smooth polygon regions of TBs

	qInfo()<< "Computing white space layout analysis...";
	Timer dt;

	if (!checkInput())
		return false;

	cv::Mat inputImg = mImg;	//not scaled

	//---------------------------------------------------------------------------------------------------------
	// PREPROCESSING: compute text height estimate, black separators and scale input image according to config options

	Timer dt0;
	//scaling of input image (if enabled: super pixels will be computed in advance)	
	if (config()->scaleInput()) {

		//Athena text height estimation
		if (config()->athenaScaling()) {
			Timer dtA;
			TextHeightEstimation the(inputImg);
		
			if (the.compute()) {
				mtextHeightAthena = the.textHeightEstimate();
				if (the.confidence() > 0.75) {
					mtextHeightEstimate = the.textHeightEstimate();
					qInfo() << "Athena text height estimation sucessfull.";
					qInfo() << "Estimated text line size = " << QString::number(mtextHeightEstimate);
					athenaEstimateValid = true;
				}
				else
					qInfo() << "Athena text height estimation has low confidence = " << QString::number(the.confidence());
			}

			QString ttA = dtA.getTotal();
			//thesis  draw
			//the.drawDebugImages(config()->debugPath());
		}

		if (mtextHeightEstimate > 0) {
			double sf = (double)mMinTextHeight / (double)mtextHeightEstimate;
			scaleInputImage(sf);
		}
		else {
			scaleInputImage();
		}			

		//thesis draw
		//drawTheDebugImages(inputImg);
	}

	QString tt0 = dt0.getTotal();
	qInfo() << "Finished preprocessing. Computation took: " << tt0;

	//---------------------------------------------------------------------------------------------------------
	// SUPER PIXEL EXTRACTION: compute initial set of text components (super pixels)
	Timer dt1;

	if (pSet.isEmpty()) {
		SuperPixel sp = computeSuperPixels(mImg);
		pSet = sp.pixelSet();
	}

	if (pSet.isEmpty()) {
		qInfo() << "No super pixels found. Finished white space analysis";
		return false;
	}

	//compute stats (needed for line spacing used in clustering/distance computation)
	if (!computeLocalStats(pSet)){
		qWarning() << "Could not compute local stats for super pixels!";
		return false;
	}

	//prune super pixels in order to reduce amout of pixels used for further processing
	filterRect = filterPixels(pSet);

	//thesis draw
	//cv::Mat unfilteredPixelImg = drawFilteredPixels(mImg, ColorManager::blue(), ColorManager::blue(), ColorManager::blue());
	//cv::Mat bigPixelImg = drawFilteredPixels(mImg, ColorManager::blue(), ColorManager::red(), ColorManager::blue());
	//cv::Mat overlappingPixelImg = drawFilteredPixels(mImg, ColorManager::blue(), QColor(), ColorManager::red());
	//cv::Mat finalPixelImg = drawFilteredPixels(mImg, ColorManager::blue(), QColor(), QColor());

	//TODO insert non-text filtering here

	//find black separators (vertical/horizontal lines) in the imput image
	if(config()->blackSeparators())
		mBlackSeparators = findBlackSeparators();

	QString tt1 = dt1.getTotal();
	qInfo() << "Finished super pixel extraction. Computation took: " << tt1;

	//---------------------------------------------------------------------------------------------------------
	// TEXT LINE HYPOTHIISIZER: compute initial text lines
	Timer dt2;

	TextLineHypothesizer tlh(mImg, pSet);
	tlh.addSeparatorLines(mBlackSeparators);
	tlh.setConfig(mTlhConfig);

	if (!tlh.compute()) {
		qWarning() << "Could not compute text line hypotheses!";
		return false;
	}

	mTextLineHypotheses = tlh.textLineSets();

	//debug outputs 
	//cv::Mat imgDebugTextLines = tlh.drawTextLineHypotheses(mImg);
	//cv::Mat imgDebugWhiteSpaces = drawWhiteSpaces(mImg);	//based on mTextLineHypotheses and computed white spaces

	qInfo() << "Found " << tlh.textLineSets().size() << " text lines.";
	
	QString tt2 = dt2.getTotal();
	qInfo() << "Finished text line hypotheses. Computation took: " << tt2;

	//---------------------------------------------------------------------------------------------------------
	// WHITE SPACE SEGMENTATION: compute white space segmentation for estimated text lines
	Timer dt3;

	//cv::Mat wsImg = drawWhiteSpaces(mImg);

	WhiteSpaceSegmentation wss(mImg, mTextLineHypotheses);
	wss.addSeparatorLines(mBlackSeparators);
	wss.setConfig(mWssConfig);

	if (!wss.compute()) {
		qWarning() << "Could not compute white space segmentation!";
		return false;
	}

	//get segmented text lines
	mWSTextLines = wss.textLineSets();

	if (config()->debugDraw()) {
		QString imgPath = Utils::createFilePath(config()->debugPath(), "_whiteSpaces","png");
		cv::Mat imgDebugWSS = wss.drawSplitTextLines(mImg);
		Image::save(imgDebugWSS, imgPath);
	}

	QString tt3 = dt3.getTotal();
	qInfo() << "Finished white space segmentation. Computation took: " << tt3;

	//---------------------------------------------------------------------------------------------------------
	// TEXT BLOCK FORMATION: compute text blocks formed by previously detected text lines
	Timer dt4;

	//use text line hypotheses as input if no WSS results are available
	if (mWSTextLines.isEmpty())
			mWSTextLines = mTextLineHypotheses;

	TextBlockFormation tbf(mImg, mWSTextLines);
	tbf.addSeparatorLines(mBlackSeparators);
	tbf.setConfig(mTbfConfig);

	if (!tbf.compute()) {
		qWarning() << "White space analysis: Could not compute text blocks!";
		return false;
	}

	mTextBlockSet = tbf.textBlockSet();

	QString tt4 = dt4.getTotal();
	qInfo() << "Finished text block formation. Computation took: " << tt4;

	//use text lines as output if no text blocks are found
	if (mTextBlockSet.isEmpty()) {
		if (mWSTextLines.isEmpty())
			mWSTextLines = mTextLineHypotheses;

		for (auto tl : mWSTextLines) {
			Rect imgRect = Rect(0, 0, mImg.cols-1, mImg.rows -1);
			QPolygonF imgPoly = Polygon::fromRect(imgRect).polygon();
			QPolygonF poly = imgPoly.intersected(tl->convexHull().polygon());	//clip polygon to image bounds

			TextBlock tb(poly);
			mTextBlockSet << tb;
		}
	}

	if (config()->debugDraw())
		drawDebugImages(mImg);

	// scale back to original coordinates
	if (config()->scaleInput()) {
		mScaleFactory->scaleInv(mTextBlockSet);
	}

	mTextBlockRegions = mTextBlockSet.toTextRegion();
	
	mInfo << "white space layout analysis computed in: " << dt;
	mInfo << "PP: " << tt0 << ", SPE: " << tt1 << ", TLH: " << tt2 << ", WSS: " << tt3 << ", TBF: " << tt4;

	return true;
}

QSharedPointer<WhiteSpaceAnalysisConfig> WhiteSpaceAnalysis::config() const {
	return qSharedPointerDynamicCast<WhiteSpaceAnalysisConfig>(mConfig);
}

void WhiteSpaceAnalysis::setTlhConfig(QSharedPointer<TextLineHypothesizerConfig> tlhConfig){
	mTlhConfig = tlhConfig;
}

void WhiteSpaceAnalysis::setWssConfig(QSharedPointer<WhiteSpaceSegmentationConfig> wssConfig){
	mWssConfig = wssConfig;
}

void WhiteSpaceAnalysis::setTbfConfig(QSharedPointer<TextBlockFormationConfig> tbfConfig){
	mTbfConfig = tbfConfig;
}

SuperPixel WhiteSpaceAnalysis::computeSuperPixels(const cv::Mat & img){

	//TODO eval choice of min max area parameters

	int mserMaxArea = (int)std::round(img.rows / 1.5);
	int mserMinArea = (int)std::round(img.rows / 52);

	//Text Spotter params
	//MSER ms(10, (int)(0.00002*mser_img.cols*mser_img.rows), (int)(0.05*mser_img.cols*mser_img.rows), 1, 0.7);

	// compute super pixels
	SuperPixel sp = SuperPixel(img);
	
	//changing sp parameters here
	auto spConfig = sp.config();
	spConfig->setNumErosionLayers(config()->numErosionLayers());
	spConfig->setMserMaxArea(mserMaxArea);
	spConfig->setMserMinArea(mserMinArea);
	sp.setConfig(spConfig);
	
	if (!sp.compute()) {
		qDebug() << "error during SuperPixel computation";
	}

	return sp;
}

bool WhiteSpaceAnalysis::computeLocalStats(PixelSet & pixels) const {

	// find local orientation per pixel
	rdf::LocalOrientation lo(pixels);
	lo.config()->setScaleFactory(mScaleFactory);

	if (!lo.compute()) {
		qWarning() << "could not compute local orientation";
		return false;
	}

	// smooth orientation
	rdf::GraphCutOrientation pse(pixels);

	if (!pse.compute()) {
		qWarning() << "could not smooth orientation";
		return false;
	}

	// smooth line spacing
	rdf::GraphCutLineSpacing pls(pixels);

	if (!pls.compute()) {
		qWarning() << "could not smooth line spacing";
		return false;
	}

	return true;
}

Rect WhiteSpaceAnalysis::filterPixels(PixelSet& set){

	//TODO increase acceptable max width (avoid pruning large character such as W, M, etc.)

	if (set.isEmpty())
		return Rect();

	QVector<QString> removeIDs;

	//filter pixels according to size constraints-------------------------------------

	QList<double> heights;
	QList<double> widths;
	for (auto p : set.pixels()) {
		widths << p->bbox().width();
		heights << p->bbox().height();
	}

	// compute bounds for widths
	double q25 = Algorithms::statMoment(widths, 0.25);
	double q50 = Algorithms::statMoment(widths, 0.5);
	double q75 = Algorithms::statMoment(widths, 0.75);
	
	double iqd = q75 - q25;
	double lbw = std::max(0.0, (q50 - iqd) / 1.5);
	double ubw = (q50 + iqd) * 2.0;

	// compute bounds for heights
	q25 = Algorithms::statMoment(heights, 0.25);
	q50 = Algorithms::statMoment(heights, 0.5);
	q75 = Algorithms::statMoment(heights, 0.75);

	double lbh = std::max(0.0, (q50 - (q75 - q25)) / 1.5);
	double ubh = (q50 + (q75 - q25)) * 2.0;

	Rect lbfR(0, 0, lbw, lbh);
	Rect ubfR(0, 0, ubw, ubh);
	//qInfo() << "lower bound filter rect = " << lbfR.toString();
	//qInfo() << "upper bound filter rect = " << ubfR.toString();

	//used for debug draw only - visualising filtering constraints
	Rect lsR(lbw, lbh, ubw, ubh);

	//remove pixel from set according to size constraints
	for (auto p : set.pixels()) {
		if (p->bbox().height() < lbh || p->bbox().width() < lbw) {
			removeIDs << p->id();
			continue;
		}

		if (p->bbox().height() > ubh || p->bbox().width() > ubw) {
			removeIDs << p->id();
		}
	}

	for (auto id : removeIDs) {
		removedPixels1 << set.find(id);
		set.remove(set.find(id));
	}

	removeIDs.clear();


	//filter overlapping pixels---------------------------------------------------------

	Timer df;
	QVector<QString> isolatedPixels;

	auto pixelGroups = findPixelGroups(set);

	//eliminate redundant pixels, based on area they cover within the group
	for (auto g : pixelGroups) {

		//process biggest pixels first, eliminate pixel if its area is covered by smaller ones
		std::sort(g.begin(), g.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->bbox().area() > rhs->bbox().area();
		});

		for (auto p1 : g){

			Rect p1R = p1->bbox();
			bool hasIntersection = false;
			QVector<QSharedPointer<Pixel>> overlappingPixels;

			//check for every pixel within the group if it is overlapping (nested) with the current one
			for (int j = 0; j < g.size(); j++) {
				
				auto p2 = g.at(j);
				QString p2ID = p2->id();
				Rect p2R = p2->bbox();

				if (isolatedPixels.contains(p2ID) || removeIDs.contains(p2ID) || (p1 == p2))
					continue;

				if (p1R.intersects(p2R))
					hasIntersection = true;
				else
					continue;

				Rect iBox = p1R.intersected(p2R);
				double relativeOverlap = iBox.area() / p2R.area();
				
				if (relativeOverlap > 0.75)
					overlappingPixels << p2;

			}

			if (!hasIntersection) {
				isolatedPixels << p1->id();
				break;
			}

			if (!overlappingPixels.isEmpty()) {

				Rect uBox = overlappingPixels[0]->bbox();
				
				if (overlappingPixels.size() > 1) {
					for (int k = 1; k < overlappingPixels.size(); k++) {
							uBox = uBox.joined(overlappingPixels[k]->bbox());
					}
				}

				Rect iBox = uBox.intersected(p1R);
				double relativeOverlap = iBox.area() / p1R.area();
				
				if (relativeOverlap > 0.75) 
					removeIDs << p1->id();
				else {
					for (auto p : overlappingPixels)
						removeIDs << p->id();
				}
			}
		}
	}

	//qDebug() << "Removing overlapping pixels t2: " << df.elapsed();
	isolatedPixels.clear();

	//remove filtered pixels from set
	for (auto id : removeIDs) {
		removedPixels2 << set.find(id);
		set.remove(set.find(id));
	}

	//qDebug() << "Removing overlapping pixels took: " << df.getTotal();
	qDebug() << "Removed " << QString::number(removeIDs.size()) << "redundant pixel(s) by analysing overlapping regions.";
	//qDebug() << "The number of remaining rectangles is: " << QString::number(set.size());

	return lsR;
}

QVector<QVector<QSharedPointer<rdf::Pixel>>> WhiteSpaceAnalysis::findPixelGroups(PixelSet& set) {

	//sort pixels for more efficient computation
	auto set_ = set.pixels();
	std::sort(set_.begin(), set_.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
	});

	//create a mask of text pixel regions
	cv::Mat mask(mImg.size(), CV_8UC1, cv::Scalar(0));

	for (auto p1 : set_) {
		mask(p1->bbox().toCvRect()) = mask(p1->bbox().toCvRect()) + 1;
	}

	cv::Mat pixel_mask = mask>0;
	cv::Mat overlap_mask = mask>1;

	//find contours of text pixel regions and their bounding box
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(pixel_mask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	QVector<Rect> pixelRects;
	for (auto c : contours) {
		QVector<Vector2D> c_;

		for (auto p : c)
			c_.push_back(Vector2D(p));

		Rect bbox = Rect::fromPoints(c_);
		bbox.setSize(bbox.size() + Vector2D(1.0, 1.0)); //TODO if this fix is still needed
		pixelRects << bbox;
	}

	//eliminate pixelRects containing only one text region
	QVector<Rect> pixelRects_;
	for (auto r : pixelRects) {
		cv::Mat overlap_r = overlap_mask(r.toCvRect());
		if (cv::countNonZero(overlap_r) > 0) {
			pixelRects_.append(r);
		}
	}

	pixelRects = pixelRects_;

	//QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);	//blank image
	//QPainter painter(&qImg);
	//
	//for (auto p : set_) {
	//	painter.setPen(ColorManager::blue());
	//	p->bbox().draw(painter);
	//}

	//for (auto r : pixelRects) {
	//	painter.setPen(ColorManager::red());
	//	r.draw(painter);
	//}

	//cv::Mat results = Image::qImage2Mat(qImg);

	//find groups of pixels that are overlapping
	std::sort(pixelRects.begin(), pixelRects.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.top() < rhs.top();
	});

	QVector<QVector<QSharedPointer<rdf::Pixel>>> pixelGroups(pixelRects.size());
	if (!pixelRects.isEmpty()) {
		for (auto p : set_) {
			Rect pR = p->bbox();

			for (int i = 0; i < pixelRects.size(); ++i) {
				Rect gR = pixelRects[i];
				if (gR.bottom() < pR.top())
					continue;

				if (gR.top() > pR.bottom())
					break;

				if (gR.contains(p->bbox())) {
					pixelGroups[i].append(p);
					break;
				}
			}
		}
	}

	//QImage qImg1(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);	//blank image
	//QPainter painter1(&qImg1);

	//for (auto g : pixelGroups) {
	//	painter1.setPen(ColorManager::randColor());
	//	for (auto p : g) {
	//		p->bbox().draw(painter1);
	//	}
	//}

	//cv::Mat results2 = Image::qImage2Mat(qImg1);
	return pixelGroups;
}

QVector<Line> WhiteSpaceAnalysis::findBlackSeparators() const {

	//TODO move parameter to config
	int mMinSeparatorMultiplier = 4; //multiplier for median pixel height -> defining min length of separators
	QVector<Line> stopLines;

	//compute median of pixel heights
	QList<double> heights;
	for (const QSharedPointer<Pixel>& px : pSet.pixels())
		heights << px->bbox().height();

	double medianPixelHeight = Algorithms::statMoment(heights, 0.5);

	LineTraceLSD lt(mImg);
	lt.config()->setScale(1.0);

	if (!lt.compute()) {
		qWarning() << "could not compute separators...";
	}

	stopLines << lt.separatorLines();

	if (medianPixelHeight > 0) {
		int minLineLength = (int) std::round(medianPixelHeight) * mMinSeparatorMultiplier;
		stopLines = lt.lineFilter().removeSmall(stopLines, minLineLength);
	}

	return stopLines;
}

QVector<QSharedPointer<TextRegion>> WhiteSpaceAnalysis::textLineRegions() const{

	QVector<QSharedPointer<TextRegion>> mTextLineRegions;
	QVector<QSharedPointer<Region> > textRegions = RegionManager::filter<Region>(mTextBlockRegions, Region::type_text_line);
	
	if (textRegions.isEmpty())
		return mTextLineRegions;

	for (auto tr : textRegions) {
		tr->setId("cvl-" + tr->id().remove("{").remove("}"));	//avoid errors when using as input for aletheia eval tool
		mTextLineRegions << qSharedPointerCast<TextRegion>(tr);
	}
	
	//for (auto tl : mTextLineRegions) {
	//	tl->setId("cvl-" + tl->id().remove("{").remove("}"));	//avoid errors when using as input for aletheia eval tool
	//}

	return mTextLineRegions;
}

QVector<QSharedPointer<TextLine>> WhiteSpaceAnalysis::textLineHypotheses() const{

	QVector<QSharedPointer<TextLine>> lineHypos;

	for (auto tl : mTextLineHypotheses) {
		auto tlr = tl->toTextLine();
		tlr->setId("cvl-" + tl->id().remove("{").remove("}"));	//avoid errors when using as input for aletheia eval tool
		lineHypos << tlr;
	}	

	return lineHypos;
}

QSharedPointer<Region> WhiteSpaceAnalysis::textBlockRegions() const {
	return mTextBlockRegions;
}

QVector<QSharedPointer<Region>> WhiteSpaceAnalysis::evalTextBlockRegions() const {
	
	QVector<QSharedPointer<Region>> evalRegions;

	for (auto tb : mTextBlockRegions->children()) {
		tb->removeAllChildren();
		tb->setId("cvl-" + tb->id().remove("{").remove("}"));	//avoid errors when using as input for aletheia eval tool
		evalRegions << tb;
	}
	
	return evalRegions;
}

cv::Mat WhiteSpaceAnalysis::draw(const cv::Mat & img, const QColor& col) const {

	// draw mser blobs
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);
	QColor tmp_col = col;
	
	for (auto tb : mTextBlockSet.textBlocks()){
		tb->draw(painter, TextBlock::DrawFlags() | TextBlock::draw_poly | TextBlock::draw_text_lines);
	}

	return Image::qImage2Mat(qImg);
}

void WhiteSpaceAnalysis::drawDebugImages(const cv::Mat & img){

	QString path = config()->debugPath();
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	//draw pixel set-------------------------------------------------
	painter.setPen(ColorManager::blue());
	pSet.draw(painter, PixelSet::draw_pixels);
	pSet.draw(painter, PixelSet::draw_rect);
	//for (auto p : pSet.pixels()) {
	//	p->bbox().draw(painter);
	//	//qDebug() << "pixel size = " << p->size() ;
	//}
	painter.end();

	QString imgPath = Utils::createFilePath(path, "_pixelSet", "png");
	cv::Mat img_debug = Image::qImage2Mat(qImg);
	Image::save(img_debug, imgPath);

	qImg = Image::mat2QImage(img, true);
	painter.begin(&qImg);

	// draw pixel set and filtered pixels--------------------------------------------------
	imgPath = Utils::createFilePath(path, "_filtered_pixels", "png");
	img_debug = drawFilteredPixels(img);
	Image::save(img_debug, imgPath);

	// draw pixel set and filtered pixels--------------------------------------------------
	imgPath = Utils::createFilePath(path, "_unfiltered_pixels", "png");
	img_debug = drawFilteredPixels(img);
	Image::save(img_debug, imgPath);

	// draw text lines-------------------------------------------------------
	for (const QSharedPointer<TextLineSet>& tl : mTextLineHypotheses) {

		painter.setPen(ColorManager::randColor());
		tl->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
	}
	
	imgPath = Utils::createFilePath(path, "_line_hypotheses", "png");
	img_debug = Image::qImage2Mat(qImg);
	Image::save(img_debug, imgPath);

	// draw final text regions-------------------------------------------------------
	imgPath = Utils::createFilePath(path, "_result_text_regions", "png");
	img_debug = draw(img);
	Image::save(img_debug, imgPath);

	// draw black separators-------------------------------------------------------
	imgPath = Utils::createFilePath(path, "_separators", "png");
	img_debug = drawBlackSeparators(img);
	Image::save(img_debug, imgPath);

}

cv::Mat WhiteSpaceAnalysis::drawWhiteSpaces(const cv::Mat & img) {

	//QImage qImg = Image::mat2QImage(img, true);
	QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	qImg.fill(ColorManager::white());
	QPainter painter(&qImg);

	for (auto tl : mTextLineHypotheses) {
		for (auto ws : tl->whiteSpacePixels()) {
			painter.setPen(ColorManager::lightGray());

			if (ws->isBCR()) {
				painter.setPen(ColorManager::red());
				ws->bbox().draw(painter);
			}	

			//ws->bbox().draw(painter);
		}

		//painter.setPen(ColorManager::blue());
		//for (auto tp : tl->pixels()) {
		//	tp->bbox().draw(painter);
		//}

		//painter.setPen(ColorManager::darkGray());
		////tl->boundingBox().draw(painter);
		//tl->convexHull().draw(painter);
	}

	cv::Mat imgDebugWhiteSpaces = Image::qImage2Mat(qImg);

	return imgDebugWhiteSpaces;
}

cv::Mat WhiteSpaceAnalysis::drawFilteredPixels(const cv::Mat & img, QColor fpCol, QColor bpCol, QColor opCol){
	
	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	//draw pixels deleted due to size contstraints
	//painter.setPen(ColorManager::darkGray());
	//Rect lb = Rect(0, 0, filterRect.left(), filterRect.top());
	//Rect ub = Rect(0, 0, filterRect.right(), filterRect.bottom());
	//lb.draw(painter);	//filter rects used as size constraint for pixels
	//ub.draw(painter);
	//painter.drawText(ub.toQRect(), Qt::AlignCenter, "fR");

	if (bpCol.isValid()) {
		painter.setPen(bpCol);
		for (auto p : removedPixels1) {
			//QString wh = QString::number(p->bbox().width()) + ", " + QString::number(p->bbox().height());
			//painter.drawText(p->bbox().topLeft().toQPoint(), wh);

			p->bbox().draw(painter);
			//p->draw(painter, 0.3, Pixel::DrawFlags() | Pixel::draw_ellipse);
		}
	}

	//draw pixels deleted due to size contstraints
	if (opCol.isValid()) {
		painter.setPen(opCol);
		for (auto p : removedPixels2) {
			p->bbox().draw(painter);
			//p->draw(painter, 0.3, Pixel::DrawFlags() | Pixel::draw_ellipse);
		}
	}

	//draw final pixel Set
	if (fpCol.isValid()) {
		painter.setPen(fpCol);
		for (auto p : pSet.pixels()) {
			p->bbox().draw(painter);
			//p->draw(painter, 0.3, Pixel::DrawFlags() | Pixel::draw_ellipse);
		}
	}

	cv::Mat img_debug = Image::qImage2Mat(qImg);
	
	return img_debug;
}

cv::Mat WhiteSpaceAnalysis::drawBlackSeparators(const cv::Mat & img) {

	if (mBlackSeparators.isEmpty())
		return img;

	//draw separators
	QImage qImg = Image::mat2QImage(img, true);
	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	QPainter painter(&qImg);

	QColor pc = ColorManager::red();
	pc.setAlphaF(0.7);

	painter.setPen(pc);

	for (auto l : mBlackSeparators) {
		Line tmp = Line(l);
		tmp.setThickness(10);
		tmp.draw(painter);
	}
		
	return Image::qImage2Mat(qImg);
}

void WhiteSpaceAnalysis::drawTheDebugImages(const cv::Mat & img) {

	// draw the debug infos-------------------------------------------------------
	cv::Mat debugImg;

	int bb = 0;
	if (img.rows < std::max(mtextHeightAthena, mtextHeightEstimate) + 100)
		bb = std::max(mtextHeightAthena, mtextHeightEstimate) + 100;
	
	cv::Mat img_ = img.clone();
	if(img_.channels() == 1)
		cv::cvtColor(img_, img_, cv::COLOR_GRAY2BGRA);

	copyMakeBorder(img_, debugImg, 0, bb, 0, 150 + mtextHeightAthena + mtextHeightEstimate, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
	cv::rectangle(debugImg, cv::Rect(img.cols + 50, 50, mtextHeightAthena, mtextHeightAthena), cv::Scalar(153, 102, 0, 255), 3); //blue
	cv::rectangle(debugImg, cv::Rect(img.cols + 100 + mtextHeightAthena, 50, mtextHeightEstimate, mtextHeightEstimate), cv::Scalar(167, 192, 120, 255), 3); //green
	
	QString path = config()->debugPath();
	QString fileName= "_the_debug_";

	if(!athenaEstimateValid)
		fileName += "_!a" + QString::number(mtextHeightAthena);
	else
		fileName += "_a" + QString::number(mtextHeightAthena);

	if (!roughEstimateValid)
		fileName += "_!r" + QString::number(mtextHeightEstimate);
	else
		fileName += "_r" + QString::number(mtextHeightEstimate);

	QString imgPath = Utils::createFilePath(path, fileName, "png");
	QImage qDebugImg = Image::mat2QImage(debugImg);

	if (!qDebugImg.save(imgPath, 0, 1))
		qDebug() << "Failed to save THE debug image.";
}

QString WhiteSpaceAnalysis::toString() const {
	return Module::toString();
}

bool WhiteSpaceAnalysis::checkInput() const {
	return !isEmpty();
}

bool WhiteSpaceAnalysis::scaleInputImage(double sf){
	
	//use original scale to find suitable scale factor
	bool scaled = false;
	if (sf == 1) {
		
		qInfo() << "Trying to find alternative rough text height estimate for scaling input image.";

		SuperPixel sp = computeSuperPixels(mImg);
		PixelSet tmpSet = sp.pixelSet();

		if (tmpSet.size() < 20) {
			qWarning()<< "Too low number of super pixels, input image will not be scaled.";
			pSet = tmpSet;
			return false;
		}

		//compute median of pixel heights
		QList<double> heights;
		for (const QSharedPointer<Pixel>& px : tmpSet.pixels())
			heights << px->bbox().height();

		double medianHeight = Algorithms::statMoment(heights, 0.5);
		
		//TODO consider adaptive scaling of median height - e.g. based on quartile distance
		//mtextHeightEstimate = medianHeight * 2.5; //old setting: tested range 1.5 - 2.5 -> 2.0 best
		mtextHeightEstimate = qRound(medianHeight * config()->textHeightMultiplier());

		//find new scale factor based on rough text height estimate
		sf =  (double)mMinTextHeight / (double)mtextHeightEstimate;

		int maxImgSide = (sf < 1) ? qRound(mImg.rows * sf) : mImg.rows;
		reconfigScaleFactory(maxImgSide);
		cv::Mat scaledImg = mScaleFactory->scaled(mImg);

		if (validateImageScale(scaledImg)) {
			mImg = scaledImg;
			roughEstimateValid = true;

			if (sf < 1)
				scaled = true;
		}
		else {
			reconfigScaleFactory(mImg.rows);
			pSet = tmpSet;

			qWarning() << "Could not find suitable scale factor for input image.";
			qInfo() << "Using full scale input image instead.";
		}

		return scaled;
	}
	else {
		int maxImgSide = (sf < 1) ? (int)round(mImg.rows * sf) : mImg.rows;
		reconfigScaleFactory(maxImgSide);
		cv::Mat scaledImg = mScaleFactory->scaled(mImg);

		if (validateImageScale(scaledImg)) {
			mImg = scaledImg;

			if (sf < 1)
				scaled = true;
		}
		else {
			qWarning() << "Scaling based on precomputed text height estimate seems inappropriate.";
			scaled = scaleInputImage();
		}

		return scaled;
	}

	//debug text file-----------------------------------------------------------------------------------

	////write scaling results to file
	//QString outputFilePath = config()->debugPath();
	//
	//QString theMethod = "_scaleInfo";
	//theMethod = (isRoughEstimate) ? QString("_scaleInfo_rthe") : QString("_scaleInfo_athena");
	//theMethod += "_sf=" + QString::number(mScaleFactory->scaleFactor()) + "_the=" + QString::number(mtextHeightEstimate);
	//outputFilePath = Utils::createFilePath(outputFilePath, theMethod, "txt");

	//QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
	//QString theInfo =	"Text Height estimation results from " + timeStamp + "\n" + 
	//					"usingAthena=" + QString::number(!isRoughEstimate) + "\n" +
	//					"sf=" + QString::number(mScaleFactory->scaleFactor()) + "\n" +
	//					 "the=" + QString::number(mtextHeightEstimate) + "\n\n";

	//QFile outputFile(outputFilePath);

	//if (outputFile.open(QFile::WriteOnly | QIODevice::Text)) {
	//	QTextStream outStream(&outputFile);
	//	outStream << theInfo << endl;
	//	outStream.flush();
	//	outputFile.close();
	//}
	//else
	//	qWarning() << "Could not open file for saving evaluation results: " << outputFilePath;

	//debug draw-----------------------------------------------------------------------------------
	
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);

	//for (auto p : pSet.pixels()) {

	//	double h = p->bbox().height();

	//	if (h >= (double)mMinTextHeight / 4.0 && h <= (double)mMinTextHeight)
	//		painter.setPen(ColorManager::blue());
	//	else
	//		painter.setPen(ColorManager::red());

	//	p->draw(painter);
	//}

	//painter.setPen(ColorManager::green());
	//rdf::Rect(0, 0, std::round(mMinTextHeight / 4.0), std::round(mMinTextHeight / 4.0)).draw(painter);
	//rdf::Rect(0, 0, std::round(mMinTextHeight), std::round(mMinTextHeight)).draw(painter);

	//painter.drawText(QPoint(100, 50), QString::number(mScaleFactory->scaleFactorDpi()));

	//QString path = config()->debugPath();
	//QString imgPath;
	//if (!isRoughEstimate)
	//	imgPath = Utils::createFilePath(path, "_sf=" + QString::number(mScaleFactory->scaleFactor()), "png");
	//else
	//	imgPath = Utils::createFilePath(path, "_r_sf=" + QString::number(mScaleFactory->scaleFactor()), "png");

	//cv::Mat debug_img = Image::qImage2Mat(qImg);
	//Image::save(debug_img, imgPath);

	//---------------------------------------------------------------------------------------------
	//qInfo() << "Input image is scaled by factor: " << mScaleFactory->scaleFactor();
}

bool WhiteSpaceAnalysis::validateImageScale(cv::Mat img){

	SuperPixel sp = computeSuperPixels(img);
	PixelSet tmpSet = sp.pixelSet();

	if (tmpSet.size() < 20)
		return false;

	auto pixels = tmpSet.pixels();
	int thCounter = 0;
	
	for (const QSharedPointer<Pixel>& px : pixels) {

		double h = px->bbox().height();

		if (h >= (double)mMinTextHeight / 4.0 && h <= (double)mMinTextHeight)
			thCounter++;
	}

	double positiveRatio = (double)thCounter / pixels.size();
	bool isValidScale = positiveRatio > 0.5;

	pSet = tmpSet;

	//debug draw-----------------------------------------------------------------------------------
	//QImage qImg = Image::mat2QImage(img, true);
	//QPainter painter(&qImg);

	//for (auto p : pSet.pixels()) {

	//	double h = p->bbox().height();

	//	if (h >= (double)mMinTextHeight / 4.0 && h <= (double)mMinTextHeight)
	//		painter.setPen(ColorManager::blue());
	//	else
	//		painter.setPen(ColorManager::red());

	//	p->draw(painter);
	//}

	//painter.setPen(ColorManager::green());
	//rdf::Rect(0, 0, std::round(mMinTextHeight / 4.0), std::round(mMinTextHeight / 4.0)).draw(painter);
	//rdf::Rect(0, 0, std::round(mMinTextHeight), std::round(mMinTextHeight)).draw(painter);

	//painter.drawText(QPoint(100, 50), QString::number(positiveRatio));

	//cv::Mat debug_img = Image::qImage2Mat(qImg);
		
	//---------------------------------------------------------------------------------------------

	return isValidScale;
}

void WhiteSpaceAnalysis::reconfigScaleFactory(int maxImgSide){

	auto sfConfig = mScaleFactory->config();
	sfConfig->setMaxImageSide(maxImgSide);
	mScaleFactory->setConfig(sfConfig);
}


// TextLineHypothesizerConfig ------------------------------------------------------------------------------

TextLineHypothesizerConfig::TextLineHypothesizerConfig() : ModuleConfig("Text Line Hypothesizer Module") {
}

QString TextLineHypothesizerConfig::toString() const {
	return ModuleConfig::toString();
}

void TextLineHypothesizerConfig::setMinLineLength(int length) {
	mMinLineLength = length;
}

int TextLineHypothesizerConfig::minLineLength() const {
	return mMinLineLength;
}

void TextLineHypothesizerConfig::setErrorMultiplier(double multiplier) {
	mErrorMultiplier = multiplier;
}

double TextLineHypothesizerConfig::errorMultiplier() const {
	return checkParam(mErrorMultiplier, 0.0, DBL_MAX, "errorMultiplier");
}

QString TextLineHypothesizerConfig::debugPath() const {
	return mDebugPath;
}

void TextLineHypothesizerConfig::load(const QSettings & settings) {
	mMinLineLength = settings.value("minLineLength", minLineLength()).toInt();
	mErrorMultiplier = settings.value("errorMultiplier", errorMultiplier()).toDouble();
	mDebugPath = settings.value("debugPath", debugPath()).toString();
}

void TextLineHypothesizerConfig::save(QSettings & settings) const {
	settings.setValue("minLineLength", minLineLength());
	settings.setValue("errorMultiplier", errorMultiplier());
	settings.setValue("debugPath", debugPath());
}


// TextLineHypothesizer -------------------------------------------------------------------------------------

TextLineHypothesizer::TextLineHypothesizer(const cv::Mat img, const PixelSet& set){
	mSet = set;
	mImg = img;
	mConfig = QSharedPointer<TextLineHypothesizerConfig>::create();
	mConfig->loadSettings();
}

bool TextLineHypothesizer::compute() {

	if (mSet.isEmpty())
		return false;

	RightNNConnector rnnpc;
	rnnpc.setStopLines(mStopLines);

	PixelGraph pg(mSet);
	pg.connect(rnnpc, PixelGraph::sort_edges);
	mPg = pg;
	
	cv::Mat results_pg = drawGraphEdges(mImg, ColorManager::blue());

	mTextLines = clusterTextLines(pg);
	//cv::Mat results1 =  draw(mImg);

	mergeUnstableTextLines(mTextLines);
	//cv::Mat results2 = draw(mImg);

	removeShortTextLines();
	//cv::Mat results3 = draw(mImg);

	for (auto tl : mTextLines) {
		extractWhiteSpaces(tl);
	}

	return true;
}

QVector<QSharedPointer<WSTextLineSet>> TextLineHypothesizer::clusterTextLines(const PixelGraph & graph) const{

	QVector<QSharedPointer<WSTextLineSet> > textLines;

	bool updated = true;
	int idx = 0;

	for (int i = graph.edges().length() - 1; i >= 0; --i) {
		auto e = graph.edges().at(i);

		double heat = 1.0 - (++idx / (double)graph.edges().size());
		updated = processEdge(e, textLines, heat);

		//Vector2D tl(537, 750);
		//Vector2D br(1060, 810);
		//Rect debugWindow = Rect(tl, br - tl);
		//updated = processEdgeDebug(e, textLines, heat, debugWindow);
	}

	return textLines;
}

bool TextLineHypothesizer::processEdge(const QSharedPointer<PixelEdge>& e, QVector<QSharedPointer<WSTextLineSet>>& textLines, double heat) const{

	int psIdx1 = findSetIndex(e->first(), textLines);
	int psIdx2 = findSetIndex(e->second(), textLines);

	if (psIdx1 == -1 && psIdx2 == -1) {										// create new text line
		if (mergePixels(e)) {
			QVector<QSharedPointer<Pixel> > px;
			px << e->first();
			px << e->second();
			textLines << QSharedPointer<WSTextLineSet>::create(px);
			return true;
		}
	}
	else if (psIdx1 == psIdx2)												// pixels are in same text line
		return false;
	else if (psIdx2 == -1) {												// add pixel to line

		if (addPixel(textLines[psIdx1], e, e->second(), heat)) {
			textLines[psIdx1]->add(e->second());
			return true;
		}
	}
	else if (psIdx1 == -1) {												// add pixel to line
		if (addPixel(textLines[psIdx2], e, e->first(), heat)) {
			textLines[psIdx2]->add(e->first());
			return true;
		}
	}
	else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2], e, heat)) {	// merge text lines
		textLines[psIdx2]->append(textLines[psIdx1]->pixels());
		textLines.remove(psIdx1);
		return true;
	}

	return false;
}

bool TextLineHypothesizer::processEdgeDebug(const QSharedPointer<PixelEdge>& e, QVector<QSharedPointer<WSTextLineSet>>& textLines, double heat, Rect debugWindow) const {

	bool updated = false;

	int psIdx1 = findSetIndex(e->first(), textLines);
	int psIdx2 = findSetIndex(e->second(), textLines);


	// debug draw------------------------------------
	cv::Mat debugImg;

	if (debugWindow.isNull())
		debugWindow = Rect(0, 0, mImg.cols, mImg.rows);

	if (debugWindow.contains(e->first()->bbox()) && debugWindow.contains(e->second()->bbox())) {

		QImage qImg = Image::mat2QImage(mImg, true);
		QPainter painter(&qImg);

		QPen dp = painter.pen();
		QPen sp = dp;
		sp.setWidth(2);
		sp.setColor(ColorManager::darkGray());
		painter.setPen(sp);
		painter.drawRect(debugWindow.toQRect());
		painter.setPen(dp);

		if (psIdx1 == -1) {
			painter.setPen(ColorManager::red());
			e->first()->draw(painter);
		}
		else {
			painter.setPen(ColorManager::lightGray());
			textLines[psIdx1]->draw(painter);
			painter.setPen(ColorManager::blue());
			e->first()->draw(painter);
		}

		if (psIdx2 == -1) {
			painter.setPen(ColorManager::red(1.0));
			e->second()->draw(painter);
		}
		else {
			painter.setPen(ColorManager::lightGray(1.0));
			textLines[psIdx2]->draw(painter);
			painter.setPen(ColorManager::blue(1.0));
			e->second()->draw(painter);
		}

		if (psIdx1 == -1 && psIdx2 == -1) {
			if (mergePixels(e))
				updated = true;
		}
		else if (psIdx1 == psIdx2) {												// pixels are in same text line
			//nothing to do
		}
		else if (psIdx2 == -1) {
			if (addPixel(textLines[psIdx1], e, e->second(), heat))
				updated = true;
		}
		// merge one pixel
		if (psIdx1 == -1 || psIdx2 == -1) {
			updated = true;
		}
		else if (psIdx1 == -1) {
			if (addPixel(textLines[psIdx2], e, e->first(), heat))
				updated = true;
		}
		else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2], e, heat)){
			updated = true;
		}
		else {
			auto tln1 = textLines[psIdx1];
			auto tln2 = textLines[psIdx2];

			Rect eBox = tln1->boundingBox().joined(tln2->boundingBox());

			sp = dp;
			sp.setWidth(2);
			sp.setColor(ColorManager::randColor(0.6));
			painter.setPen(sp);

			tln1->line().extendBorder(eBox).draw(painter);
			for (auto p : tln1->centers()) {
				painter.drawEllipse(p.toQPointF(), 4, 4);
				painter.drawText((int)p.x()-4, (int)p.y()-15, QString::number(std::round(tln2->line().distance(p))));
			}

			sp.setColor(ColorManager::randColor(0.6));
			painter.setPen(sp);

			tln2->line().extendBorder(eBox).draw(painter);
			for (auto p : tln2->centers()) {
				painter.drawEllipse(p.toQPointF(), 4, 4);
				painter.drawText((int)p.x()-4, (int)p.y()-15, QString::number(std::round(tln1->line().distance(p))));
			}

			painter.setPen(dp);
		}
		
		// draw edge according to clustering result
		if (updated)
			painter.setPen(ColorManager::blue(1.0));
		else
			painter.setPen(ColorManager::red(1.0));

		e->draw(painter);

		debugImg = Image::qImage2Mat(qImg);
	}
	
	// debug draw end---------------------------------------------------------------------

	// create a new text line
	if (psIdx1 == -1 && psIdx2 == -1) {
		if (mergePixels(e)) {
			QVector<QSharedPointer<Pixel> > px;
			px << e->first();
			px << e->second();
			textLines << QSharedPointer<WSTextLineSet>::create(px);
			updated = true;
		}
	}
	else if (psIdx1 == psIdx2) {
		updated = false;
	}
	// merge one pixel
	else if (psIdx2 == -1) {

		if (addPixel(textLines[psIdx1], e, e->second(), heat)) {
			textLines[psIdx1]->add(e->second());
			updated = true;
		}
	}
	// merge one pixel
	else if (psIdx1 == -1) {
		if (addPixel(textLines[psIdx2], e, e->first(), heat)) {
			textLines[psIdx2]->add(e->first());
			updated = true;
		}
	}
	// merge to same text line
	else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2], e, heat)) {
		textLines[psIdx2]->append(textLines[psIdx1]->pixels());

		textLines.remove(psIdx1);
		updated = true;
	}

	return updated;
}

void TextLineHypothesizer::extractWhiteSpaces(QSharedPointer<WSTextLineSet>& textLine) const {

	//TODO simplify this section
	//TODO move this section to the white space segmenation class

	QVector<Rect> textRects;
	for (auto p : textLine->pixels()) {
		textRects << p->bbox();
	}

	QVector<Rect> mergedRects;
	QVector<int> processedIdx;
	
	//merge text region rects that are overlapping (needed for white space extraction)
	for (int i = 0; i < textRects.size(); ++i) {
		if (processedIdx.contains(i))
			continue;

		Rect tr1 = textRects[i];
		bool merged = false;

		if (!mergedRects.isEmpty()) {
			for (int j = 0; j < mergedRects.size(); ++j) {
				Rect tr2 = mergedRects[j];

				if (tr1.left() <= tr2.right() && tr2.left() <= tr1.right()) {
					processedIdx << i;
					mergedRects[j] = tr1.joined(tr2);
					merged = true;
					break;
				}
			}
		}

		if (!merged) {
			for (int j = 0; j < textRects.size(); ++j) {

				if (i == j || processedIdx.contains(j))
					continue;

				Rect tr2 = textRects[j];

				if (tr1.left() <= tr2.right() && tr2.left() <= tr1.right()) {
					processedIdx << i;
					processedIdx << j;

					mergedRects << tr1.joined(tr2);
					merged = true; 
					break;
				}
			}
		}

		if (!merged) {
			processedIdx << i;
			mergedRects << textRects[i];
		}
	}

	//sort text rects according to their x coordinates
	std::sort(mergedRects.begin(), mergedRects.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.left() < rhs.left();
	});
	
	//remove overlapping rects
	for (int i = 0; i < mergedRects.size(); ++i) {
		Rect m1 = mergedRects[i];
		for (int j = 0; j < mergedRects.size(); ++j) {
			Rect m2 = mergedRects[j];
			if (i != j) {
				if (m2.left() <= m1.right() && m1.left() <= m2.right()) {
					mergedRects[i] = m1.joined(m2);
					mergedRects.remove(j);
				}
			}
		}
	}

	//sort text rects according to their x coordinates
	std::sort(mergedRects.begin(), mergedRects.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.right() < rhs.right();
	});

	////debug draw------------------------------------------
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter p(&qImg);
	//
	//p.setPen(ColorManager::lightGray());
	//for (auto r : textRects) {
	//	r.draw(p);
	//}

	//p.setPen(ColorManager::blue());
	//for (auto r : mergedRects) {
	//	r.draw(p);
	//}
	//p.end();

	//cv:: Mat mergeResults = Image::qImage2Mat(qImg);
	////debug draw------------------------------------------

	QVector<QSharedPointer<WhiteSpacePixel>> whiteSpaces;

	for (int i = 1; i < mergedRects.size(); ++i) {
		Rect r1 = mergedRects[i - 1];
		Rect r2 = mergedRects[i];

		double width = r2.left() - r1.right();

		if (width > 2) { //ignore rects with 0 width (i. e. lines), can't fit ellipse for lines

			double x = r1.right() + 1;
			double y = std::max(r1.top(), r2.top());
			double height = std::max((std::min(r1.bottom(), r2.bottom())) - y, 3.0);	//min height 3 (for ellipse)
			width = width - 2;
			Rect wsr(x, y, width, height);

			cv::Point2f tl = wsr.topLeft().toCvPoint2f();
			cv::Point2f tr = wsr.topRight().toCvPoint2f();
			cv::Point2f br = wsr.bottomRight().toCvPoint2f();
			Ellipse ellipse(cv::RotatedRect(tl, tr, br));

			QSharedPointer<WhiteSpacePixel> wsp(new WhiteSpacePixel(ellipse, wsr));
			whiteSpaces << wsp;
		}
	}

	//indentify bcr white spaces
	QVector<QSharedPointer<WhiteSpacePixel>> ws_temp(whiteSpaces);

	std::sort(ws_temp.begin(), ws_temp.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().width() > rhs->bbox().width();
	});

	//compute max number of between column rectangles and mark them
	//TODO compute this parameter in the text line creation
	double pageWidth = (double)mImg.cols;
	double tlcWdith = textLine->boundingBox().width();
	double bcrn = std::max(1.0, std::round(8 * tlcWdith / pageWidth));	//assumed number of between column rects

	if (ws_temp.length() < bcrn)
		bcrn = ws_temp.size();

	for (int i = 0; i < bcrn; ++i) {
		ws_temp.at(i)->setBCR(true);
	}

	//add white spaces to text line;
	textLine->setWhiteSpacePixels(whiteSpaces);

	//compute approximate lower bound for size of bcr
	//double minBCRSize= textLine->pixelWidth()*0.5;
	double minBCRSize = textLine->pixelWidth();
	textLine->setMinBCRSize(minBCRSize);
}

int TextLineHypothesizer::findSetIndex(const QSharedPointer<Pixel> &pixel, const QVector<QSharedPointer<WSTextLineSet>> &sets) const{

	assert(pixel);

	if (!sets.empty()) {
		for (int idx = 0; idx < sets.size(); idx++) {
			if (sets[idx]->contains(pixel))
				return idx;
		}
	}

	return -1;
}

bool TextLineHypothesizer::mergePixels(const QSharedPointer<PixelEdge> &e) const {
	//TODO consider reducing maxHeightRaio parameter (higher robustness in pixel merging process)
	//TODO check if maxHeightRatio is obsolete when using relative vertical overlap threshold

	double maxHeightRatio = 3.0;
	double minVertOverlapRatio = 0.4;

	//filter pixel according to x/y overlap
	Rect p1R = e->first()->bbox();
	Rect p2R = e->second()->bbox();

	//check height ratio
	double heightRatio = p1R.height() / p2R.height();

	if ((1 / maxHeightRatio) < heightRatio && heightRatio < maxHeightRatio) {

		double yOverlap = std::min(p1R.bottom(), p2R.bottom()) - std::max(p1R.top(), p2R.top());
		double relYoverlap = yOverlap / std::max(p1R.height(), p2R.height());

		if (relYoverlap <= 0) {
			qWarning() << "Linked pixels are not overlapping. Please check the pixel graph computation.";
			return false;
		}

		if(relYoverlap > minVertOverlapRatio) {
			return true;
		}
	}
	else {
		qDebug() << "This should not happen";
	}

	return false;

}

bool TextLineHypothesizer::addPixel(QSharedPointer<WSTextLineSet> &set, const QSharedPointer<PixelEdge> &e, const QSharedPointer<Pixel> &p, double heat) const{

	if (set->size() < 10) {
		return mergePixels(e);
	}
	else {
		Rect bbox = set->boundingBox().joined(p->bbox());
		Line cLine = set->line().extendBorder(bbox);
		
		Line lLine(p->bbox().topLeft(), p->bbox().bottomLeft());
		lLine = lLine.extendBorder(Rect(0, 0, mImg.cols, mImg.rows));
		Vector2D lcp1 = cLine.intersection(lLine);

		Line rLine(p->bbox().topRight(), p->bbox().bottomRight());
		rLine = rLine.extendBorder(Rect(0, 0, mImg.cols, mImg.rows));
		Vector2D lcp2 = cLine.intersection(rLine);

		if (lcp1.isNull() || lcp2.isNull()) {
			qWarning() << "Could not add pixel, expected intersection point lays beyond image borders.";
			return false;
		}

		double avgPH = set->avgPixelHeight();

		std::vector<cv::Point> pts;
		pts.push_back(Vector2D(lcp1.x(), lcp1.y() + (avgPH / 2)).toCvPoint());
		pts.push_back(Vector2D(lcp1.x(), lcp1.y() - (avgPH / 2)).toCvPoint());
		pts.push_back(Vector2D(lcp2.x(), lcp2.y() - (avgPH / 2)).toCvPoint());
		pts.push_back(Vector2D(lcp2.x(), lcp2.y() + (avgPH / 2)).toCvPoint());

		cv::Mat mask(mImg.size(), CV_8UC1, cv::Scalar(0));
		cv::fillConvexPoly(mask, pts, cv::Scalar(1.0));
		cv::Rect pbox = p->bbox().toCvRect() + cv::Size(1, 1); //TODO check if this expansion is still needed
		mask(pbox) = mask(pbox) + 1;

		cv::Mat inter_mask = mask > 1;
		cv::Mat union_mask = mask > 0;

		double inter_count = cv::countNonZero(inter_mask);
		double union_count = cv::countNonZero(union_mask);

		double jIndex = inter_count/union_count;

		//debug draw start----------------------------------------------------------------------------------------------------------------
		//QImage qImg = Image::mat2QImage(mImg, true);
		////QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);
		//QPainter painter(&qImg);

		//Line tLine = cLine.moved(Vector2D(0, (avgPH / 2)));
		//Line bLine = cLine.moved(Vector2D(0, -(avgPH / 2)));

		//painter.setPen(ColorManager::darkGray().lighter());
		//rLine.draw(painter);
		//lLine.draw(painter);

		//painter.setPen(ColorManager::blue().lighter());
		//tLine.draw(painter);
		//bLine.draw(painter);

		////painter.setPen(ColorManager::blue());
		////cLine.draw(painter);

		////pixel to be added
		//painter.setPen(ColorManager::darkGray().lighter());
		//Polygon::fromRect(p->bbox()).draw(painter);

		////intersection rect
		//painter.setPen(ColorManager::blue().lighter());
		//Polygon poly = Polygon::fromCvPoints(pts);
		//poly.draw(painter);

		////text lines poly
		//painter.setPen(ColorManager::blue().lighter());
		//set->convexHull().draw(painter);

		////connecting edge
		//QPen pen(ColorManager::darkGray());
		//pen.setWidth(4);
		//painter.setPen(pen);

		//e->first()->center().draw(painter);
		//e->second()->center().draw(painter);

		//if (jIndex < 0.5)
		//	painter.setPen(ColorManager::red());
		//else
		//	painter.setPen(QColor(100,200,0,255));

		//e->draw(painter);

		//cv::Mat debug_result = Image::qImage2Mat(qImg);
		////debug_result.setTo(cv::Scalar(150, 150, 150, 256), union_mask);
		////debug_result.setTo(cv::Scalar(75, 75, 75, 256), inter_mask);
		//debug draw end----------------------------------------------------------------------------------------------------------------

		if (jIndex > 0.5) {
			return true;
		}
	}
	
	return false;
}

bool TextLineHypothesizer::mergeTextLines(const QSharedPointer<WSTextLineSet>& tls1, const QSharedPointer<WSTextLineSet>& tls2, const QSharedPointer<PixelEdge> &e, double heat) const{

	//TODO add additional check for horizontal overlap of text lines (avoid merging neighboring text lines , due to touching components)
	//TODO incorporate average size of lines in linking process
	//TODO evaluate parameter settings for: minAgreeRatio
	//TODO improve linking of short components

	if (tls1->size() < 10 && tls1->size() < 10) {
		return	mergePixels(e);
	}

	// sort line pixels according to distance from center of the linked line
	QVector<QSharedPointer<Pixel>> pixels1 = tls1->pixels();
	Vector2D c1 = tls1->center();

	QVector<QSharedPointer<Pixel>> pixels2 = tls2->pixels();
	Vector2D c2 = tls2->center();

	if (c1.x() < c2.x()) {
		std::sort(pixels1.begin(), pixels1.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->center().x() > rhs->center().x();
		});
		std::sort(pixels2.begin(), pixels2.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->center().x() < rhs->center().x();
		});
	}
	else{
		std::sort(pixels1.begin(), pixels1.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->center().x() < rhs->center().x();
		});
		std::sort(pixels2.begin(), pixels2.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->center().x() > rhs->center().x();
		});
	}

	bool mergeLines = false;

	if (tls1->size() > tls2->size()) {
		mergeLines = isContinuousMerge(tls1, pixels2);

		if (!mergeLines) {
			if (tls2->size() < 6 && mergePixels(e))
				mergeLines = true;
		}
	}
	else {
		mergeLines = isContinuousMerge(tls2, pixels1);

		if (!mergeLines) {
			if (tls1->size() < 6 && mergePixels(e))
				mergeLines = true;
		}
	}


	//debug draw start-------------------------------------------------------------------------------
	//QImage qImg = Image::mat2QImage(mImg, true);
	////QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);
	//QPainter painter(&qImg);
	//QPen gPen(QColor(100, 200, 0, 255));
	//QPen rPen(ColorManager::red());

	//Rect unionBox = tls1->boundingBox().joined(tls2->boundingBox());

	//// draw lines
	//painter.setPen(ColorManager::blue());
	//tls1->convexHull().draw(painter);
	//tls2->convexHull().draw(painter);

	//QVector<QSharedPointer<Pixel>> pixels;
	//QSharedPointer<WSTextLineSet> tls1_;
	//QSharedPointer<WSTextLineSet> tls2_;

	//if (tls1->size() >= tls2->size()) {
	//	tls1_ = tls1;
	//	tls2_ = tls2;
	//	pixels = pixels2;
	//}
	//else {
	//	tls1_ = tls2;
	//	tls2_ = tls1;
	//	pixels = pixels1;
	//}

	//double maxDist = (tls1_->avgPixelHeight() / 2.0);
	//Line cLine = tls1_->line().extendBorder(unionBox);
	//Line tLine = cLine.moved(Vector2D(0, maxDist));
	//Line bLine = cLine.moved(Vector2D(0, -maxDist));

	//painter.setPen(ColorManager::blue().lighter());
	//cLine.draw(painter);

	//painter.setPen(QPen(ColorManager::blue().lighter(), 1, Qt::DashLine));
	//tLine.draw(painter);
	//bLine.draw(painter);

	//int idxBound = std::min(5, pixels.size());

	//for (int i = 0; i < idxBound; ++i) {
	//	auto p = pixels[i];
	//	double pixDist = tls1_->line().distance(pixels[i]->center());

	//	if (pixDist < maxDist)
	//		painter.setPen(gPen);
	//	else
	//		painter.setPen(rPen);

	//	p->bbox().draw(painter);

	//	QPen tmpPen = painter.pen();
	//	QPen pen_(tmpPen);
	//	pen_.setWidth(4);
	//	painter.setPen(pen_);

	//	p->center().draw(painter);
	//	painter.setPen(tmpPen);

	//	//painter.drawText(p->center().x() - 4, p->center().y() - 15, QString::number(std::round(tls1_->line().distance(p->center()))));
	//}

	//if (mergeLines)
	//	painter.setPen(gPen);
	//else
	//	painter.setPen(rPen);

	//e->draw(painter);

	//cv::Mat reuslts = Image::qImage2Mat(qImg);
	//cv::Mat croppedResults = reuslts(unionBox.toCvRect());

	//debug draw end-------------------------------------------------------------------------------
	
	return mergeLines;
}

bool TextLineHypothesizer::isContinuousMerge(const QSharedPointer<WSTextLineSet>& tls, const QVector<QSharedPointer<Pixel>>& pixels) const{

	double agreements = 0;
	double idxBound = std::min(5, pixels.size());
	double maxDist = (tls->avgPixelHeight() / 2.0);

	for (int i = 0; i < idxBound; ++i) {
		
		double pixDist = tls->line().distance(pixels[i]->center());

		if (pixDist < maxDist)
			++agreements;
	}

	double agreeRatio = agreements / idxBound;

	if (agreeRatio > minAgreeRatio)
		return true;
	else
		return false;

}

void TextLineHypothesizer::mergeUnstableTextLines(QVector<QSharedPointer<WSTextLineSet>>& textLines) const {

	//TODO fix parameters
	//TODO simplify merging of components that vertically overlapping and are close to each other
	//TODO add additional check for horizontal overlap of text lines (avoid merging neighboring text lines , due to nearby components)

	//void TextLineHelper::mergeStableTextLines(QVector<QSharedPointer<TextLineSet>>& textLines)

	QVector<QSharedPointer<TextLineSet>> textLines_;
	for (auto tl : textLines)
		textLines_.append(qSharedPointerCast<TextLineSet>(tl));

	QVector<QSharedPointer<TextLineSet>> unstable = TextLineHelper::filterAngle(textLines_);

	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);
	//for (auto tl : unstable) {
	//	painter.setPen(ColorManager::randColor());
	//	tl->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
	//}
	//cv::Mat ulImg = Image::qImage2Mat(qImg);
	
	double tlExtFactor = 1.2;

	QVector<Polygon> polys;
	for (const auto tl : textLines) {
		polys << tl->convexHull();
	}

	//merge unstable Lines with existing text lines if they are close to each other
	int numRemoved = 0;
	for (int uIdx = 0; uIdx < unstable.size(); uIdx++) {

		auto utl = qSharedPointerCast<WSTextLineSet>(unstable[uIdx]);

		// compute left & right points (w.r.t text orientation)
		Ellipse el = utl->fitEllipse();
		Vector2D vl = el.getPoint(0);
		Vector2D vr = el.getPoint(CV_PI);

		// compute extended points
		vl = el.center() + (el.center() - vl)*tlExtFactor;
		vr = el.center() + (el.center() - vr)*tlExtFactor;

		double cErr = DBL_MAX;
		int bestIdx = -1;
		QVector<Vector2D> pts = utl->centers();

		// find merging candidates (textlines that contain the right/left most point)
		for (int idx = 0; idx < textLines.size(); idx++) {

			if (textLines[idx]->id() == utl->id())
				continue;

			// find all candidate textlines
			if (polys[idx].contains(vl) || polys[idx].contains(vr)) {

				Line mLine = Line(textLines[idx]->center(), textLines[uIdx]->center());

				//TODO move separator check to own function - used again later on
				bool crossesSeparator = false;
				for (const Line& line : mStopLines) {
					if (mLine.intersects(line)) {
						crossesSeparator = true;
						break;
					}
				}

				if (crossesSeparator)
					continue;

				double err = textLines[idx]->computeError(pts);

				if (err < cErr) {
					bestIdx = idx;
					cErr = err;
				}
			}
		}

		// merge
		if (bestIdx != -1) {

			if (utl->size() > 2)
				textLines[bestIdx]->append(utl->pixels());

			int rIdx = textLines.indexOf(utl);
			textLines.remove(rIdx);
			polys.remove(rIdx);
			numRemoved++;
		}
	}

	qDebug() << "Merged " << numRemoved << " unstable text lines.";

	//merge small text lines with others if they are covered by their convex hull
	QVector<QSharedPointer<WSTextLineSet>> unstableLines;
	for (auto tl1 : textLines) {
		for (auto tl2 : textLines) {

			if (tl1->id() == tl2->id())
				continue;

			Rect tl1R = tl1->boundingBox();
			Rect tl2R = tl2->boundingBox();

			if (tl1R.area() > tl2R.area()) {
				if (tl1R.intersects(tl2R)) {

					Polygon interPoly = Polygon(tl1->convexHull().polygon().intersected(tl2->convexHull().polygon()));

					if (interPoly.size() == 0)
						continue;

					//check if majority of centers is lying within current lines - check #1
					Polygon poly = tl1->convexHull();
					int pCount = 0;
					for (auto p : tl2->pixels()) {
						if (poly.contains(p->center()))
							pCount++;
					}
					double coverage = pCount / tl2->size();
					if (coverage > 0.75) {
						tl1->append(tl2->pixels());
						unstableLines << tl2;
					}


					//remove text lines if >75% of its area is covered by another line - - check #2
					cv::Mat mask(mImg.size(), CV_8UC1, cv::Scalar(0));
					Rect interRect = tl1R.joined(tl2R);

					std::vector<cv::Point>  pts_inter = interPoly.toCvPoints();
					std::vector<cv::Point>  pts_tl2 = tl2->convexHull().toCvPoints();

					cv::fillConvexPoly(mask, pts_tl2, cv::Scalar(1.0));
					cv::fillConvexPoly(mask, pts_inter, cv::Scalar(2.0));
					
					mask = mask(interRect.toCvRect());
					cv::Mat tl2_mask = mask > 0;
					cv::Mat inter_mask = mask == 2;
					
					mask = tl2_mask/255*100;
					mask = mask + inter_mask/255*150;

					coverage = cv::countNonZero(inter_mask)/(double) cv::countNonZero(tl2_mask);

					if (coverage > 0.75) {
						tl1->append(tl2->pixels());
						unstableLines << tl2;
						continue;
					}
				}
			}
		}
	}

	qDebug() << "Removed " << unstableLines.size() << " small text lines covered by bigger lines.";

	for (auto ul : unstableLines) {
		int idx = textLines.indexOf(ul);

		if (idx != -1)
			textLines.remove(idx);
	}
	
	//debug
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);

	numRemoved = 0;

	//TODO move this part to own function, mergeLines (could be used again inlater stages)
	//merge text lines if they are close to each other and have similar size and orientation
	for (int i = 0; i < textLines.size(); ++i) {
		auto tl1 = textLines[i];

		for (int j = i +1; j < textLines.size();) {
			auto tl2 = textLines[j];

			Rect tl1R = tl1->boundingBox();
			Rect tl2R = tl2->boundingBox();

			double overlap = std::min(tl1R.bottom(), tl2R.bottom()) - std::max(tl1R.top(), tl2R.top());
			if (overlap > 0) {
				double relOverlap = overlap / std::max(tl1R.height(), tl2R.height());

				//check if separator is inbetween
				Line mLine = Line(tl1->center(), tl2->center());
				bool crossesSeparator = false;
				for (const Line& line : mStopLines) {
					if (mLine.intersects(line)) {
						crossesSeparator = true;
						break;
					}
				}

				if (crossesSeparator) {
					++j;
					continue;
				}

				if (relOverlap > 0.5) {

					double xDistance = std::max(tl1R.left(), tl2R.left()) - std::min(tl1R.right(), tl2R.right());
					double aph = (tl1->avgPixelHeight() + tl2->avgPixelHeight()) /2.0;
					
					//check horizontal distance of text lines
					if (xDistance < (5 * aph)) {
						double maxErr1 = std::max(tl1->error() * config()->errorMultiplier(), tl1->avgPixelHeight() / 2.0);
						double maxErr2 = std::max(tl2->error() * config()->errorMultiplier(), tl2->avgPixelHeight() / 2.0);

						double nErr1 = tl1->computeError(tl2->centers());
						double nErr2 = tl2->computeError(tl1->centers());

						//debug
						//painter.setPen(ColorManager::blue());
						//tl1->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
						//painter.setPen(ColorManager::red());
						//tl2->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);

						//check if merge of lines is reasonable
						if (nErr1 < maxErr1 && nErr2 < maxErr2) {

							tl1->mergeWSTextLineSet(tl2);
							textLines.remove(j);
							++numRemoved;

							//debug
							//painter.setPen(ColorManager::lightGray(0.2));
							//tl1->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
							//cv::Mat mergedLinesImg = Image::qImage2Mat(qImg);

							continue; //process next tls
						}
					}
					//debug
					//cv::Mat mergedLinesImg = Image::qImage2Mat(qImg);
				}
			}
			++j;
		}
	}

	qDebug() << "Merged " << numRemoved << " nearby text lines having similar size and orientation.";
}

void TextLineHypothesizer::removeShortTextLines() {
	//TODO fix parameter settings
	int minTextLineSize = config()->minLineLength();

	QVector<QSharedPointer<WSTextLineSet>> removeTl;
	for (auto tl : mTextLines) {
		if (tl->size() < minTextLineSize) {
			removeTl << tl;
		}
	}

	for (auto tl : removeTl) {
		mTextLines.remove(mTextLines.indexOf(tl));
	}
}

QVector<QSharedPointer<TextLine>> TextLineHypothesizer::textLines() const {

	QVector<QSharedPointer<TextLine>> tls;
	for (auto set : mTextLines)
		tls << set->toTextLine();

	return tls;
}

QVector<QSharedPointer<WSTextLineSet>> TextLineHypothesizer::textLineSets() const {
	return mTextLines;
}

bool TextLineHypothesizer::isEmpty() const{
	return mSet.isEmpty();
}

void TextLineHypothesizer::addSeparatorLines(const QVector<Line>& lines) {
	mStopLines << lines;
}

bool TextLineHypothesizer::checkInput() const {
	return !mSet.isEmpty();
}

QSharedPointer<TextLineHypothesizerConfig> TextLineHypothesizer::config() const {
	return qSharedPointerDynamicCast<TextLineHypothesizerConfig>(mConfig);
}

cv::Mat TextLineHypothesizer::draw(const cv::Mat& img, const QColor& col) const{

	QImage qImg = Image::mat2QImage(img, true);
	cv::Mat img_final;

	QPainter p(&qImg);
	p.setPen(col);

	if (!col.isValid())
		p.setPen(ColorManager::randColor());

	// draw text lines
	img_final = drawTextLineHypotheses(img);

	return img_final;
}

cv::Mat TextLineHypothesizer::drawGraphEdges(const cv::Mat& img, const QColor& col) {
	
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);
	QPen pen(col);
	
	QPen pen1(ColorManager::red());

	QPen pen2(col);
	pen2.setWidth(4);

	painter.setPen(pen);

	//mPg.draw(painter);

	//rnn ex 1: 1000, 1347 - 1023, 1381 
	//rnn ex 2: 452, 1986 - 472, 2032
	Rect r = Rect(452, 1986, 472 - 452, 2032 - 1986);

	for (auto p : mSet.pixels())
		p->bbox().draw(painter);

	for (auto e : mPg.edges()) {
		
		//if (!r.contains(e->first()->bbox()))
		//	continue;

		painter.setPen(pen1);
		e->draw(painter);
			
		painter.setPen(pen2);
		painter.drawPoint(e->first()->center().toQPointF());
		painter.drawPoint(e->second()->center().toQPointF());
	}

	return Image::qImage2Mat(qImg);
}

cv::Mat TextLineHypothesizer::drawTextLineHypotheses(const cv::Mat& img) const{
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	for (auto tl : mTextLines) {

		//QPen pen = ColorManager::blue(0.5);
		//pen.setWidth(3);
		//painter.setPen(pen);

		//for (auto p : tl->pixels()) {
		//	p->bbox().draw(painter);
		//}

		//painter.setPen(ColorManager::red());
		//for (auto ws : tl->whiteSpacePixels()) {
		//	ws->bbox().draw(painter);
		//}

		painter.setPen(ColorManager::randColor());
		tl->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
		//tl->boundingBox().draw(painter);
	}

	return Image::qImage2Mat(qImg);
}

// WhiteSpaceSegmentationConfig ------------------------------------------------------------------------------

WhiteSpaceSegmentationConfig::WhiteSpaceSegmentationConfig() : ModuleConfig("White Space Segmentation Module") {
}

QString WhiteSpaceSegmentationConfig::toString() const {
	return ModuleConfig::toString();
}

void WhiteSpaceSegmentationConfig::setMinLengthMultiplier(double mlm) {
	mMinLengthMultiplier = mlm;
}

double WhiteSpaceSegmentationConfig::minLengthMultiplier() const {
	return checkParam(mMinLengthMultiplier, 0.0, DBL_MAX, "minLengthMultiplier");
}

void WhiteSpaceSegmentationConfig::setSlicingSizeMultiplier(double ssm) {
	mSlicingSizeMultiplier = ssm;
}

double WhiteSpaceSegmentationConfig::slicingSizeMultiplier() const {
	return checkParam(mSlicingSizeMultiplier, 0.0, DBL_MAX, "slicingSizeMultiplier");
}

void WhiteSpaceSegmentationConfig::setFindWhiteSpaceGaps(bool wsGaps){
	mFindWhiteSpaceGaps = wsGaps;
}

bool WhiteSpaceSegmentationConfig::findWhiteSpaceGaps() const{
	return mFindWhiteSpaceGaps;
}

void WhiteSpaceSegmentationConfig::load(const QSettings & settings) {
	mSlicingSizeMultiplier = settings.value("minLengthMultiplier", minLengthMultiplier()).toDouble();
	mMinLengthMultiplier = settings.value("slicingSizeMultiplier", slicingSizeMultiplier()).toDouble();
	mFindWhiteSpaceGaps = settings.value("findWhiteSpaceGaps", findWhiteSpaceGaps()).toBool();
}

void WhiteSpaceSegmentationConfig::save(QSettings & settings) const {
	settings.setValue("minLengthMultiplier", minLengthMultiplier());
	settings.setValue("slicingSizeMultiplier", slicingSizeMultiplier());
	settings.setValue("findWhiteSpaceGaps", findWhiteSpaceGaps());
}

// WhiteSpaceSegmentation --------------------------------------------------------------------

WhiteSpaceSegmentation::WhiteSpaceSegmentation() {
	mConfig = QSharedPointer<WhiteSpaceSegmentationConfig>::create();
}

WhiteSpaceSegmentation::WhiteSpaceSegmentation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>>& tlsM) {
	mImg = img;
	mInitialTls = tlsM;
	mConfig = QSharedPointer<WhiteSpaceSegmentationConfig>::create();
}

bool WhiteSpaceSegmentation::isEmpty() const {
	return mTlsM.isEmpty();
}

bool WhiteSpaceSegmentation::compute() {

	//TODO consider incorporating white separators at the beginning of the segmentation process

	if (mInitialTls.isEmpty()) {
		qInfo("White space segmentation skipped. There are no text line to be processed.");
		return true;
	}

	//split text lines based on white spaces
	if (!splitTextLines()) {
		qInfo("Finished white space segmentation, no splitting of text lines required.");
		return true;
	}

	if (mTlsM.isEmpty() || mBcrM.isEmpty()) {
		qWarning() << "Warning: No text lines or white spaces found for segmentation!";
		return false;
	}
	
	//debug images
	//cv::Mat graphResult, splitTextLines, noIsolatedTextLines, noShortTextLines, initialSplitLines;
	//initialSplitLines = drawSplitTextLines(mImg);	

	bool updatedTextLines = true;

	while (updatedTextLines) {
		mPg = computeSegmentationGraph();
		//graphResult = drawPixelGraph(mImg);

		updatedTextLines = removeIsolatedBCR(mPg);
		//noIsolatedTextLines = drawSplitTextLines(cv::Mat());
		
		updatedTextLines = mergeShortTextLines() || updatedTextLines;
		//noShortTextLines = drawSplitTextLines(mImg);
	}

	//splitTextLines = drawSplitTextLines(mImg);

	if (!findWhiteSpaceRuns(mPg)) {
		qInfo("Finished white space segmentation, no white space runs found.");
		return true;
	}

	//cv::Mat whiteSpaceRuns = drawWhiteSpaceRuns(mImg);

	bool updatedSegmentation = true;
	while (updatedSegmentation) {

		//update status of remaining BCR
		updateBCRStatus();

		//cv::Mat intermediateTextLines1 = drawSplitTextLines(mImg);
		//cv::Mat intermediatewhiteSpaceRuns1 = drawWhiteSpaceRuns(mImg);

		//remove groups that represent non-bcr runs or are too short
		updatedSegmentation = refineWhiteSpaceRuns();

		//cv::Mat intermediateTextLines2 = drawSplitTextLines(mImg);
		//cv::Mat intermediatewhiteSpaceRuns2 = drawWhiteSpaceRuns(mImg);
	}

	//cv::Mat segmentedTextLines = drawSplitTextLines(mImg);
	//cv::Mat finalWhiteSpaceRuns = drawWhiteSpaceRuns(mImg);
	
	//split text lines crossing elongated vertical (white space) gaps
	if (config()->findWhiteSpaceGaps()) {
		mWhiteSeparatorLines = computeWhiteSpaceGaps();
		splitAtWhiteSeparators();
		//cv::Mat whiteSeparators = drawWhiteSeparators(segmentedTextLines);
	}

	refineTextLineResults();

	return true;
}

QSharedPointer<WhiteSpaceSegmentationConfig> WhiteSpaceSegmentation::config() const{
	return qSharedPointerDynamicCast<WhiteSpaceSegmentationConfig>(mConfig);
}

bool WhiteSpaceSegmentation::checkInput() const {
	return !mTlsM.isEmpty();
}

bool WhiteSpaceSegmentation::splitTextLines(){

	//TODO simplify this section using new splitWSTextLineSet function

	//split text lines according to white spaces (bcr)	
	for (auto tl : mInitialTls) {

		QVector<QSharedPointer<Pixel>> textPixels(tl->pixels());
		QVector<QSharedPointer<WhiteSpacePixel>> wsPixels(tl->whiteSpacePixels());

		if (wsPixels.isEmpty()) {
			mTlsM << tl;
			continue;
		}
		
		//sort pixels according to x position
		std::sort(textPixels.begin(), textPixels.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->bbox().right() < rhs->bbox().right();
		});
		
		std::sort(wsPixels.begin(), wsPixels.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->bbox().right() < rhs->bbox().right();
		});

		//split current text line at white spaces (bcr only)
		int lastTPidx = 0;
		int lastWSidx = 0;

		for (int i = 0; i < wsPixels.size(); ++i) {
			auto ws = wsPixels[i];

			if (ws->isBCR()) {
				mBcrM << ws;

				for (int j = lastTPidx; textPixels.size(); ++j) {
					if (textPixels[j]->bbox().right() > ws->bbox().left()) {

						QSharedPointer<WSTextLineSet> tls;
						QVector<QSharedPointer<Pixel>> stp = textPixels.mid(lastTPidx, (j - lastTPidx));

						if (i == 0 || i == (wsPixels.size() - 1) || i == lastWSidx) { //add white space?
							tls = QSharedPointer<WSTextLineSet>::create(stp);
							tls->setMinBCRSize(tl->minBCRSize());
							mTlsM << tls;
						}
						else {
							QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx, (i - lastWSidx));
							tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
							tls->setMinBCRSize(tl->minBCRSize());
							mTlsM << tls;
						}

						//save indices of text line sets neighboring bcr
						if (lastWSidx != 0) {
							QString key = mBcrM[mBcrM.size() - 2]->id();
							QVector<QSharedPointer<WSTextLineSet>> value = mBcrNeighbors.value(key);
							value << tls;
							mBcrNeighbors.insert(key, value);
						}

						QVector<QSharedPointer<WSTextLineSet>> value(1, tls);
						mBcrNeighbors.insert(ws->id(), value);

						lastTPidx = j;
						lastWSidx = i + 1;
						break;
					}
				}
			}
		}

		//add text line containing pixels after last bcr
		QSharedPointer<WSTextLineSet> tls;
		QVector<QSharedPointer<Pixel>> stp = textPixels.mid(lastTPidx);

		if (lastWSidx > (wsPixels.size() - 1)) {	//bcr is last white space in text line
			tls = QSharedPointer<WSTextLineSet>::create(stp);
			tls->setMinBCRSize(tl->minBCRSize());
			mTlsM << tls;
		}
		else {
			QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx);
			tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
			tls->setMinBCRSize(tl->minBCRSize());
			mTlsM << tls;
		}

		QString key = mBcrM[mBcrM.size() - 1]->id();
		QVector<QSharedPointer<WSTextLineSet>> value = mBcrNeighbors.value(key);
		value << tls;
		mBcrNeighbors.insert(key, value);
	}

	if (mBcrM.isEmpty()) {
		return false;
	}

	return true;
}

double WhiteSpaceSegmentation::computeLineSpacing() const{

	QList<double> spacings;
	for (auto tl : mTlsM) {
		for (auto p : tl->pixels()) {
			spacings << p->bbox().height();
		}
	}

	return Algorithms::statMoment(spacings, 0.5);
}

PixelGraph WhiteSpaceSegmentation::computeSegmentationGraph() const{

	//TODO check if distance between pixels could be reduced (only search for one line below/above)

	// compute pixel graph for segmentation regions
	PixelSet pgSet;
	//QVector<QSharedPointer<Pixel>> trSet;

	//create pixel set for white space computation (text regions + BCR)
	for (auto sl : mTlsM) {
		QSharedPointer<Pixel> trPixel = sl->convertToPixel();
		//trSet << trPixel;
		//wsSet.add(qSharedPointerCast<Pixel>(trPixel));
		pgSet.add(trPixel);
	}

	for (auto bcr : mBcrM) {
		pgSet.add(qSharedPointerCast<Pixel>(bcr));
	}

	//find horizontally overlapping white spaces
	WSConnector wspc;
	wspc.setLineSpacing(computeLineSpacing());
	wspc.setStopLines(mSeparatorLines);

	PixelGraph pg(pgSet);
	pg.connect(wspc);

	return pg;
}

bool WhiteSpaceSegmentation::removeIsolatedBCR(PixelGraph pg) {

	//TODO check if 3/4 coverage is enough for isolation of BCR (affecting erronous tls or border regions)
	double extFactor = 3.5;
	QVector<QSharedPointer<WhiteSpacePixel>> isolatedBCR;
	
	for (auto bcr : mBcrM) {

		bool topIsIsolated = false;
		bool bottomIsIsolated = false;
		bool hasNeighboringWS = false;

		for (int idx : pg.edgeIndexes(bcr->id())) {
			auto linkedPixel = pg.edges()[idx]->second();

			if (mBcrM.contains(qSharedPointerCast<WhiteSpacePixel>(linkedPixel))) {
				hasNeighboringWS = true;
				break;
			}
			
			Rect lpRect = linkedPixel->bbox();
			Rect bcrRect = bcr->bbox();
			//check if top/bottom of bcr rect is covered by linked text regions
			if (lpRect.left() <= bcrRect.left() && lpRect.right() >= bcrRect.right()) {
				if (linkedPixel->center().y() < bcr->center().y())
					topIsIsolated = true;
				else if (linkedPixel->center().y() > bcr->center().y())
					bottomIsIsolated = true;
			}
		}

		if (topIsIsolated && bottomIsIsolated) {
			isolatedBCR << bcr;
		}
		else if (!hasNeighboringWS) {
			
			auto neighbors = mBcrNeighbors.value(bcr->id());
			double maxRemoveSize = 0;

			for (auto n : neighbors) {
				double tmpMax = n->minBCRSize()*extFactor;
				if (maxRemoveSize < tmpMax)
					maxRemoveSize = tmpMax;
			}

			if (maxRemoveSize > 0 && bcr->bbox().width() > maxRemoveSize) {
				continue;
			}

			double minGap = 0;
			for (auto tl : mBcrNeighbors.value(bcr->id())) {
				//double curGap = std::max(tl->maxGap()*1.33, tl->minBCRSize()*0.33); // TODO test different settings for extFactor
				double curGap = std::max(tl->maxGap()*1.33, tl->minBCRSize()*2.0);
				if (minGap < curGap)
					minGap = curGap;
			}

			if (minGap >= bcr->bbox().width()) {
				isolatedBCR << bcr;
				continue;
			}

			for (auto tls : mInitialTls) {
				if (tls->containsWhiteSpace(bcr)) {
					int bcrCounter = 0;
					for (auto ws : tls->whiteSpacePixels()) {
						if (ws->bbox().width() * 1.3 > bcr->bbox().width())
							++bcrCounter;
					}

					double bcrn = std::max(1.0, std::round(8 * tls->boundingBox().width() / (double) mImg.cols));
					if (bcrCounter >= bcrn)	//check if bcr size is uncommon
						isolatedBCR << bcr;
						
					break;
				}
			}
		}
	}

	//remove isolated bcr (and merge neighboring text lines)
	if (!isolatedBCR.isEmpty()) {
		deleteBCR(isolatedBCR);
		//qInfo() << "There are " << mBcrM.size() << " white spaces remaining";

		return true;
	}
	else
		return false;
}

bool WhiteSpaceSegmentation::mergeShortTextLines(){

	//debug draw--------------------------------------------------------------------------
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);

	//for (auto tls : mTlsM) {

	//	if (tls->isShort()) {
	//		QPen ppen = ColorManager::blue();
	//		ppen.setWidth(2);
	//		painter.setPen(ppen);
	//	}
	//	else{
	//		QPen ppen = ColorManager::pink();
	//		ppen.setWidth(2);
	//		painter.setPen(ppen);
	//	}

	//	tls->boundingBox().draw(painter);
	//	Rect minBCRRect = Rect(tls->boundingBox().topLeft(), Vector2D(tls->minBCRSize(), tls->minBCRSize()));
	//	minBCRRect.draw(painter);

	//	for (auto ws : tls->whiteSpacePixels()) {
	//		QPen ppen1 = ColorManager::red();
	//		ppen1.setWidth(2);
	//		painter.setPen(ppen1);

	//		//QString text = QString::number(ws->bbox().width());
	//		//painter.drawText(ws->bbox().toQRect(), Qt::AlignCenter, text);
	//		ws->bbox().draw(painter);
	//	}

	//	painter.setPen(ColorManager::blue());
	//	QString text = QString::number(tls->maxGap()) + "/" + QString::number(tls->minBCRSize()*0.33) + "/" + QString::number(tls->size());
	//	painter.drawText(tls->boundingBox().toQRect(), Qt::AlignCenter, text);
	//}

	//cv::Mat resultIsShort = Image::qImage2Mat(qImg);
	//debug draw--------------------------------------------------------------------------
	double extFactor = 2.0;
	//double extFactor = 1.33;
	bool removedBCR = false;
	QVector<QSharedPointer<WhiteSpacePixel>> revomableBCR;

	//merge short text lines enclosed by bcrs with nearest text line
	for (auto tls : mTlsM) {
		if (tls->isShort()){
			QVector<QSharedPointer<WhiteSpacePixel>> tlsNeighbors;

			for (auto bcr : mBcrM) {

				auto neighbors = mBcrNeighbors.value(bcr->id());

				if (neighbors.contains(tls))
					tlsNeighbors.append(bcr);

				if (tlsNeighbors.size() == 2)
					break;
			}

			if (tlsNeighbors.size() == 2) {

				double maxRemoveSize = tls->minBCRSize()*extFactor;
				double bcr1Size = tlsNeighbors[0]->bbox().width();
				double bcr2Size = tlsNeighbors[1]->bbox().width();
				
				if ( bcr1Size < maxRemoveSize || bcr2Size < maxRemoveSize ) {
					if ( bcr1Size < bcr2Size )
						revomableBCR << tlsNeighbors[0];
					else
						revomableBCR << tlsNeighbors[1];
				}
			}
		}
	}

	if (!revomableBCR.isEmpty()) {
		deleteBCR(revomableBCR);
		revomableBCR.clear();
		removedBCR = true;
	}

	//revoke bcr status for white spaces that are enclosed by short text lines
	for(auto bcr : mBcrM){
		auto neighbors = mBcrNeighbors.value(bcr->id());
		bool hasOnlyShortNeighbors = true;

		double maxRemoveSize = 0;
		for (auto n : neighbors) {
			
			if (maxRemoveSize < n->minBCRSize()*extFactor)
				maxRemoveSize = n->minBCRSize()*extFactor;

			if (!n->isShort()) {
				hasOnlyShortNeighbors = false;
				break;
			}			
		}

		if (hasOnlyShortNeighbors) {
			if(maxRemoveSize > 0 && bcr->bbox().width() < maxRemoveSize){
				revomableBCR << bcr;
			}
		}
	}

	if (!revomableBCR.isEmpty()) {
		deleteBCR(revomableBCR);
		return true;
	}
	else
		return removedBCR;
}

void WhiteSpaceSegmentation::deleteBCR(const QVector<QSharedPointer<WhiteSpacePixel>>& bcrM) {
	for (auto p : bcrM) {
		deleteBCR(p);
	}
}

void WhiteSpaceSegmentation::deleteBCR(const QSharedPointer<WhiteSpacePixel>& bcr) {

	if (!mBcrM.contains(bcr))
		return; //bcr not found (already deleted)
	
	bcr->setBCR(false);

	//merge text lines neighboring the removable bcr
	auto ntl = mBcrNeighbors.value(bcr->id());
	mBcrNeighbors.remove(bcr->id());

	if (ntl.size() != 2) {
		qWarning() << "Deleting BCR but number of neighboring text lines is not equal to 2!";
		mBcrM.remove(mBcrM.indexOf(bcr));
		return;
	}

	if (ntl.size() == 2) {
		ntl[0]->addWhiteSpace(bcr);
		ntl[0]->mergeWSTextLineSet(ntl[1]);

		//update mBcrNeighbors
		for (auto bcr_tmp : mBcrNeighbors.keys()) {
			if (mBcrNeighbors.value(bcr_tmp).contains(ntl[1])) {
				auto  tmp_ntl = mBcrNeighbors.value(bcr_tmp);
				int idx = tmp_ntl.indexOf(ntl[1]);
				tmp_ntl.replace(idx, ntl[0]);
				mBcrNeighbors.insert(bcr_tmp, tmp_ntl);
			}
		}

		int idx = mTlsM.indexOf(ntl[1]);
		mTlsM.remove(idx);

		//delete bcr
		mBcrM.remove(mBcrM.indexOf(bcr));
	}
}

bool WhiteSpaceSegmentation::findWhiteSpaceRuns(const PixelGraph pg) {
	auto wsSet = mBcrM;

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(wsSet.begin(), wsSet.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
	});

	QMap<QString, QVector<int>> edgeIndices;
	QMap < QString, QVector<int>> udCount;
	QVector<QSharedPointer<PixelEdge>> bcrEdges;

	for (auto e : pg.edges()) {
		if (mBcrM.contains(qSharedPointerCast<WhiteSpacePixel>(e->first()))) {
			if (mBcrM.contains(qSharedPointerCast<WhiteSpacePixel>(e->second()))) {
				bcrEdges << e;

				auto key = e->first()->id();
				auto v1 = edgeIndices.value(key);
				v1 << bcrEdges.indexOf(e);
				edgeIndices.insert(key, v1);

				if (e->first()->center().y() > e->second()->center().y()) {
					auto v2 = udCount.value(key);

					if (v2.isEmpty())
						v2 << 1 << 0;
					else
						v2[0] = ++v2[0];

					udCount.insert(key, v2);
				}
				else {
					auto v2 = udCount.value(key);

					if (v2.isEmpty())
						v2 << 0 << 1;
					else
						v2[1] = ++v2[1];

					udCount.insert(key, v2);
				}
			}
		}

	}

	for (auto ws : wsSet) {

		if (!udCount.contains(ws->id())) {
			continue;
		}

		int upCount = udCount.value(ws->id())[0];
		int downCount = udCount.value(ws->id())[1];

		// if ws is not connected to other ws above start new run
		// if ws is connected to multiple ws belows start new run each of them
		if (upCount == 0 || downCount > 1) {

			//start white space run for each downward edge
			for (int idx : edgeIndices.value(ws->id())) {

				Rect fR = bcrEdges[idx]->first()->bbox();
				Rect sR = bcrEdges[idx]->second()->bbox();

				//qInfo() << "fR = " << fR.toString();
				//qInfo() << "sR = " << sR.toString();

				if (fR.center().y() < sR.center().y()) {

					QSharedPointer<WhiteSpaceRun> wsr = QSharedPointer<WhiteSpaceRun>::create();
					wsr->add(ws);

					auto p = bcrEdges[idx]->second();
					int nextBCR_idx = mBcrM.indexOf(qSharedPointerCast<WhiteSpacePixel>(p));

					//TODO simplify this section
					while (true) {
						auto ws_tmp = mBcrM.at(nextBCR_idx);
						nextBCR_idx = -1;

						wsr->add(ws_tmp);
						int downCount2 = udCount.value(ws_tmp->id())[1];
						
						//trace white space run further
						if (downCount2 == 1) {

							for (int idx2 : edgeIndices.value(ws_tmp->id())) {
								Rect fR2 = bcrEdges[idx2]->first()->bbox();
								Rect sR2 = bcrEdges[idx2]->second()->bbox();
								if (fR2.bottom() < sR2.top()) {
									auto ws_tmp3 = qSharedPointerCast<WhiteSpacePixel>(bcrEdges[idx2]->second());
									nextBCR_idx = mBcrM.indexOf(ws_tmp3);
									continue;
								}
							}
							if (nextBCR_idx == -1) {
								qWarning() << "Couldn't find next element of white space run.";
								break;
							}
						}
						else   //reached end of white space run
							break;
					}
					mWsrM << wsr;
				}
			}
		}
	}

	//qInfo() << "Found " << mWsrM.size() << " white space runs.";

	if (mWsrM.isEmpty())
		return false;
	else
		return true;
}

void WhiteSpaceSegmentation::updateBCRStatus() {

	//TODO set parameter in config;
	double gapExtFactor = 1.3;

	for (auto wsr : mWsrM) {
		double maxGapWsr = 0;

		for (auto ws : wsr->whiteSpaces()) {
			for (auto tl : mBcrNeighbors.value(ws->id())) {
				double maxGap = std::max(tl->maxGap(), tl->minBCRSize());
				if (maxGapWsr < maxGap) {
					maxGapWsr = maxGap;
				}
			}
		}

		double croppedWsGap = 0, tmpGap = 0;
		for(int i=0; i < wsr->whiteSpaces().size(); ++i){
			auto ws = wsr->whiteSpaces()[i];
			//double wsGap = ws->bbox().width();

			//TODO debug and test this section
			if (i + 1 < wsr->whiteSpaces().size()) {
				auto ws2 = wsr->whiteSpaces()[i + 1];
				
				Rect r1 = ws->bbox();
				Rect r2 = ws2->bbox();

				double overlap = std::min(r1.right(), r2.right()) - std::max(r1.left(),r2.left());

				if (tmpGap == 0 || tmpGap > overlap)
					croppedWsGap = overlap;
				else
					croppedWsGap = tmpGap;

				tmpGap = overlap;
			}

			if (croppedWsGap < maxGapWsr*gapExtFactor) {
				ws->setBCR(false);
				continue;
			}
		}
	}
}

bool WhiteSpaceSegmentation::refineWhiteSpaceRuns() {

	//preprocessing for white space removal
	QMap<QString, int> wsrCountMap;	//for each white space count # of wsr that contain it
	QMap<QString, int> bcrCountMap; //for each white space run count # of bcr in it

	for (auto wsr : mWsrM) {
		for (auto ws : wsr->whiteSpaces()) {
			if (!bcrCountMap.contains(wsr->id()))
				if (ws->isBCR())
					bcrCountMap.insert(wsr->id(), 1);
				else
					bcrCountMap.insert(wsr->id(), 0);
			else {
				if (ws->isBCR()) {
					int c = 1 + bcrCountMap.value(wsr->id());
					bcrCountMap.insert(wsr->id(), c);
				}
			}

			if (!wsrCountMap.contains(ws->id()))
				wsrCountMap.insert(ws->id(), 1);
			else {
				int c = 1 + wsrCountMap.value(ws->id());
				wsrCountMap.insert(ws->id(), c);
			}
		}
	}

	//find white space runs that are obsolete and remove them
	QVector<QSharedPointer<WhiteSpaceRun>> obsoleteWhiteSpaceRuns;
	int wsRemoveCount = 0;

	for (auto wsr : mWsrM) {
		QVector<QSharedPointer<WhiteSpacePixel>> obsoleteWhiteSpaces;
		int bcrCount = bcrCountMap.value(wsr->id());

		if (wsr.isNull() || wsr->size() == 0) {
			qWarning() << "Found WSR with no white spaces! This should not happen!";
			continue;
		}

		//delete whole white space run if no or only one bcr is contained
		if (bcrCount <= 1) {
			obsoleteWhiteSpaces << wsr->whiteSpaces();
			obsoleteWhiteSpaceRuns << wsr;
		}
		else {
			//trim first and last element of wsr if they are no bcr
			auto firstWS = wsr->whiteSpaces().first();

			//remove first ws if it's not a bcr
			if (!firstWS->isBCR()) {
				obsoleteWhiteSpaces << firstWS;
				wsr->remove(firstWS);
			}

			//remove last ws if it's not a bcr and wsr is longer than 1
			if (wsr->whiteSpaces().size() > 1) {
				auto lastWS = wsr->whiteSpaces().last();

				if (!lastWS->isBCR()) {
					obsoleteWhiteSpaces << lastWS;
					wsr->remove(lastWS);
				}
			}

			//if wsr is trimmed down to size 1, remove whole wsr
			if (wsr->whiteSpaces().size() == 1) {
				obsoleteWhiteSpaces << wsr->whiteSpaces();
				obsoleteWhiteSpaceRuns << wsr;
			}
		}

		//delete white spaces
		for (auto ws : obsoleteWhiteSpaces) {
			int wsrCount = wsrCountMap.value(ws->id());

			if (wsrCount == 1) {
				deleteBCR(ws);
				wsrCountMap.insert(ws->id(), 0);
				++wsRemoveCount;
			}

			if (wsrCount > 1)
				wsrCountMap.insert(ws->id(), wsrCount - 1);
		}
	}

	//delete white space runs
	for (auto wsr : obsoleteWhiteSpaceRuns) {
		mWsrM.remove(mWsrM.indexOf(wsr));
	}

	//qInfo() << "Deleted " << obsoleteWhiteSpaceRuns.size() << " white space runs.";

	if (wsRemoveCount > 0) {
		return true;
	}

	return false;
}

QVector<Line> WhiteSpaceSegmentation::computeWhiteSpaceGaps() {

	//TODO test skew estimation based on estimated text lines
	//TODO evaluate influence of minLength/slicingSize multiplier variables on segmentation performance
	//TODO create debug draw function and move code from main function

	//int minLengthMultiplier = 4;
	//int slicingSizeMultiplier = 6;

	//double minLengthMultiplier = config()->minLengthMultiplier();
	//double slicingSizeMultiplier = config()->slicingSizeMultiplier();

	double minLengthMultiplier = 2.5 * 3;
	double slicingSizeMultiplier = 2.5 * 3;

	//qDebug() << "minLengthMultiplier = " << minLengthMultiplier;
	//qDebug() << "slicingSizeMultiplier = " << slicingSizeMultiplier;

	bool debugDraw = false;
	cv::Mat debugImg_hpp = cv::Mat::zeros(mImg.size(), CV_8UC1);

	cv::Mat binImg = cv::Mat::zeros(mImg.size(), CV_8UC1);
	cv::Mat tlImg = binImg.clone();
	cv::Mat wsImg = binImg.clone();

	///////////
	//stage 1: deskewing and preprocessing

	double documentSkew = 0;
	double normWeight = 0;
	double tlhE = 0;

	QVector<double> weightedAngles;

	//compute document skew according to main/average text line orientation
	for (auto tl : mInitialTls) {
		Line l = tl->line();
		weightedAngles << l.angle()*l.length();
		normWeight += l.length();

		tlhE += tl->avgPixelHeight(); //avg line height computation

		//drawing approximated text line image
		cv::fillConvexPoly(tlImg, tl->convexHull().toCvPoints(), cv::Scalar(255));
		for (auto p : tl->pixels())
			binImg(p->bbox().toCvRect()) = 255;

		//qDebug() << "angle = " << weightedAngles.last()* DK_RAD2DEG << " * length = " << (double)l.length();
	}

	for (double a : weightedAngles)
		documentSkew += a / normWeight;

	documentSkew = std::round(documentSkew * DK_RAD2DEG);
	tlhE = tlhE / mInitialTls.size();

	qDebug() << "Average text line height (in pixels) is " << tlhE;
	qDebug() << "Document skew (in degrees) is " << std::round(documentSkew) << " according to weighted text line angles.";

	//create text region mask
	int tlhE_ = qRound(tlhE);
	int border = qRound(tlhE_ * 2.5 * 2.0) + 1;

	cv::Mat tlImg_buffer;
	copyMakeBorder(tlImg, tlImg_buffer, border, border, border, border, cv::BORDER_CONSTANT, cv::Scalar(0));

	cv::Mat kernel = cv::Mat::ones(qRound(tlhE_*1.25), border-1, CV_8UC1);
	cv::morphologyEx(tlImg_buffer, tlImg_buffer, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1, cv::BORDER_CONSTANT, cv::Scalar(0));

	kernel = cv::Mat::ones(1, tlhE_ * 2, CV_8UC1);
	cv::dilate(tlImg_buffer, tlImg_buffer, kernel, cv::Point(-1, -1), 1, cv::BORDER_CONSTANT, cv::Scalar(0));
	tlImg = cv::Mat(tlImg_buffer, cv::Rect(border, border, tlImg.cols, tlImg.rows));
	
	//deskewing of binarized input image
	cv::Point2f rotCenter(mImg.cols*0.5f, mImg.rows*0.5f);

	if (documentSkew != 0) {
		cv::Mat rotMat = cv::getRotationMatrix2D(rotCenter, documentSkew, 1.0);
		cv::warpAffine(binImg, binImg, rotMat, mImg.size(), cv::INTER_CUBIC);
		cv::warpAffine(tlImg, tlImg, rotMat, mImg.size(), cv::INTER_CUBIC);
	}

	///////////
	//stage 2: compute local vertical projection profiles
	cv::Mat slice, vPP, vPPMask;

	//int slicingSize = qRound(tlhE*2.5) * 3;
	int slicingSize = qRound(tlhE * slicingSizeMultiplier);
	int startIdx = 0;
	int endIdx = slicingSize;

	while (true) {
		slice = binImg.rowRange(startIdx, endIdx);
		reduce(slice, vPP, 0, CV_REDUCE_SUM, CV_32F);

		vPPMask = vPP == 0;
		for (int i = startIdx; i < endIdx; ++i)
			vPPMask.copyTo(wsImg.row(i));

		if (debugDraw) {
			cv::Mat sliceDebugImg = cv::Mat::zeros(slice.size(), CV_8UC1);
			int max_y = slice.rows - 1;

			for (int j = 0; j < vPP.cols; j++) {
				int hppVal = (int)round(vPP.at<float>(j) / 255);

				if (hppVal == 0)
					continue;

				cv::Point p1 = cv::Point(j, max_y);
				cv::Point p2 = cv::Point(j, max_y - hppVal);
				cv::line(sliceDebugImg, p1, p2, cv::Scalar(255), 1, 8, 0);
			}
			sliceDebugImg.copyTo(debugImg_hpp(cv::Range(startIdx, endIdx), cv::Range::all()));
		}

		if (endIdx == (mImg.size().height))
			break;

		startIdx = endIdx;
		endIdx = endIdx + slicingSize;

		if (endIdx >= mImg.size().height) {
			endIdx = mImg.size().height;
			startIdx = mImg.size().height - slicingSize;
		}
	}

	///////////
	//stage 3: determine vertical white space gaps (lines) separating text regions
	//TODO check if morphological operations are needed

	//int minLength = (int)round(tlhE*2.5) * 3;
	int minLength = qRound(tlhE * minLengthMultiplier);

	//multiply white space image with text line region mask -> gap areas splitting current lines
	cv::Mat vWSR = wsImg.mul(tlImg);

	//refine white separator regions
	kernel = cv::Mat::ones(1, 7, CV_8UC1);//eliminate narrow gaps
	cv::morphologyEx(vWSR, vWSR, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1, cv::BORDER_CONSTANT, cv::Scalar(0));

	kernel = cv::Mat::ones((int)round(tlhE*1.25), 1, CV_8UC1);//vertically connect nearby gap regions
	cv::morphologyEx(vWSR, vWSR, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1, cv::BORDER_CONSTANT, cv::Scalar(0));

	kernel = cv::Mat::ones(minLength, 1, CV_8UC1); //remove short gap regions	
	cv::morphologyEx(vWSR, vWSR, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1, cv::BORDER_CONSTANT, cv::Scalar(0));

	//find vertical lines in white separator region mask
	LineTraceLSD lt(vWSR == 0);
	lt.config()->setScale(1.0); //avoid down scaling

	if (!lt.compute())
		qWarning() << "could not compute separators...";

	//filter lines (by orientation and length)
	QVector<Line> vLines = lt.lines();
	vLines = lt.lineFilter().filterLineAngle(vLines, CV_PI*0.5, 10 * DK_DEG2RAD);	//TODO reduce angleDiff parameter (test)
	vLines = lt.lineFilter().removeSmall(vLines, minLength);

	if (vLines.isEmpty())
		return vLines;

	//rotate lines back according to estimated document skew
	if (documentSkew != 0) {
		for (int i = 0; i < vLines.size(); ++i) {
			Line l = vLines[i];
			QVector<Vector2D> pts = { l.p1(), l.p2() };

			for (int j = 0; j < pts.size(); ++j) {
				Vector2D pRot = (pts[j] - Vector2D(rotCenter));
				pRot.rotate(-documentSkew * DK_DEG2RAD);
				pts[j] = pRot + Vector2D(rotCenter);
			}
			vLines.replace(i, Line(pts[0], pts[1]));
		}
	}

	return vLines;
}

void WhiteSpaceSegmentation::splitAtWhiteSeparators() {

	if (mWhiteSeparatorLines.isEmpty())
		return;

	for (auto sl : mWhiteSeparatorLines) {

		QVector<QSharedPointer<WSTextLineSet>> splitLines;
		QVector<QSharedPointer<WSTextLineSet>> removableLines;

		for (auto tl : mTlsM) {
			Line bl = tl->line();

			QSharedPointer<WhiteSpacePixel> splitPixel;
			if (sl.intersects(bl)) {
				int ix = (int)round(sl.intersection(bl).x());	//x coordinate of intersection
				for (auto wsp : tl->whiteSpacePixels()) {
					if (wsp->bbox().left() <= ix+3 && ix-3 <= wsp->bbox().right()) {
						splitPixel = wsp;
						break;
					}						
				}
			}

			if (!splitPixel.isNull()) {
				auto newLines = tl->splitWSTextLineSet(splitPixel);
				if (!newLines.isEmpty()) {
					splitLines << newLines;
					removableLines << tl;
				}
			}
		}

		//update text lines list accroding to splits
		for (auto tl : removableLines)
			mTlsM.remove(mTlsM.indexOf(tl));

		mTlsM << splitLines;
	}

	//cv::Mat resultsFinal =  drawSplitTextLines(mImg);
	//resultsFinal = drawWhiteSeparators(resultsFinal);
}

bool WhiteSpaceSegmentation::refineTextLineResults() {

	//remove text lines containing only one text element
	QVector<QSharedPointer<WSTextLineSet>> removeTl;
	for (auto tl : mTlsM) {
		if (tl->isEmpty() || tl->size() < 3) {
			removeTl << tl;
		}
	}

	if (!removeTl.isEmpty()) {
		for (auto tl : removeTl) {
			mTlsM.remove(mTlsM.indexOf(tl));
		}
		return true;
	}

	return false;
}

QVector<QSharedPointer<WSTextLineSet>> WhiteSpaceSegmentation::textLineSets() const {
	return mTlsM;
}

QVector<QSharedPointer<WhiteSpacePixel>> WhiteSpaceSegmentation::bcrSet() const{
	return mBcrM;
}

void WhiteSpaceSegmentation::addSeparatorLines(const QVector<Line>& lines) {
	mSeparatorLines << lines;
}

cv::Mat WhiteSpaceSegmentation::draw(const cv::Mat & img, const QColor & col) {

	return img;
}

cv::Mat WhiteSpaceSegmentation::drawSplitTextLines(const cv::Mat & img, const QColor & col){

	QImage qImg;

	if (img.empty()) {
		qImg = QImage(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);
		qImg.fill(ColorManager::white());
	}
	else
		qImg = Image::mat2QImage(img, true);

	QPainter painter(&qImg);

	for (auto tl : mTlsM) {
		QPen pen = ColorManager::blue();
		pen.setWidth(3);
		painter.setPen(pen);
		//tl->boundingBox().draw(painter);
		tl->convexHull().draw(painter);
		//QString maxGap = QString::number(tl->maxGap());
		//painter.drawText(tl->boundingBox().toQRect(), Qt::AlignCenter, maxGap);

		//for (auto ws : tl->whiteSpacePixels()) {
		//	painter.setPen(ColorManager::darkGray());
		//	ws->bbox().draw(painter);
		//	QString wsGap = QString::number(ws->bbox().width());
		//	painter.drawText(ws->bbox().toQRect(), Qt::AlignCenter, wsGap);
		//}
	}
	
	//for (auto bcr : mBcrM) {
	//	if(bcr->isBCR())
	//		painter.setPen(ColorManager::red());
	//	else
	//		painter.setPen(ColorManager::darkGray());

	//	bcr->bbox().draw(painter);
	//	QString wsGap = QString::number(bcr->bbox().width());
	//	painter.drawText(bcr->bbox().toQRect(), Qt::AlignCenter, wsGap);
	//}

	cv::Mat output = Image::qImage2Mat(qImg);

	//additionally draw white separator
	//output = drawWhiteSeparators(output);
	
	return output;
}

cv::Mat WhiteSpaceSegmentation::drawPixelGraph(const cv::Mat & img, const QColor & col) {
	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image

	if (!mPg.isEmpty()) {

		cv::Mat splitLines = drawSplitTextLines(img);
		QImage qImg = Image::mat2QImage(splitLines, true);
		QPainter painter(&qImg);
		
		if (!col.isValid())
			painter.setPen(col);
		else
			painter.setPen(ColorManager::green());

		mPg.draw(painter);
		
		return Image::qImage2Mat(qImg);
	}

	return img;
}

cv::Mat WhiteSpaceSegmentation::drawWhiteSpaceRuns(const cv::Mat & img, const QColor & col){

	//QImage qImg = Image::mat2QImage(img, true);
	QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);
	qImg.fill(ColorManager::white());
	QPainter painter(&qImg);

	for (auto wsr : mWsrM) {
		for (auto bcr : wsr->whiteSpaces()) {
			if (bcr->isBCR())
				painter.setPen(ColorManager::red());
			else
				painter.setPen(ColorManager::darkGray());

			bcr->bbox().draw(painter);
		}
	}

	painter.setPen(ColorManager::blue());
	for (auto wsr : mWsrM) {
		Rect wsrBox = wsr->boundingBox();
		double lx = wsrBox.center().x();
		Line l(lx, wsrBox.top(), lx, wsrBox.bottom());
		l.draw(painter);
	}

	return Image::qImage2Mat(qImg);
}

cv::Mat WhiteSpaceSegmentation::drawWhiteSeparators(const cv::Mat & img, const QColor & col) {
	cv::Mat inImg = img.clone();
	QImage qImg = Image::mat2QImage(inImg, true);
	QPainter painter(&qImg);

	if (!col.isValid())
		painter.setPen(ColorManager::red());
	else
		painter.setPen(col);

	for (auto l : mWhiteSeparatorLines) {
		Line l_ = l;
		l_.setThickness(3);
		l_.draw(painter);
	}
	
	return Image::qImage2Mat(qImg);
}

// TextBlockFormationConfig--------------------------------------------------------------------
TextBlockFormationConfig::TextBlockFormationConfig() : ModuleConfig("Text Block Formation Module") {
}

QString TextBlockFormationConfig::toString() const {
	return ModuleConfig::toString();
}

void TextBlockFormationConfig::setPolygonType(int pt){
	mPolygonType = pt;
}

int TextBlockFormationConfig::polygonType() const{
	return ModuleConfig::checkParam(mPolygonType, 0, INT_MAX, "polygonType");
}

void TextBlockFormationConfig::load(const QSettings & settings){
	mPolygonType = settings.value("polygonType", polygonType()).toInt();
}

void TextBlockFormationConfig::save(QSettings & settings) const{
	settings.setValue("polygonType", polygonType());
}

// TextBlockFormation --------------------------------------------------------------------

TextBlockFormation::TextBlockFormation() {
	mConfig = QSharedPointer<TextBlockFormationConfig>::create();
	mConfig->loadSettings();
}

TextBlockFormation::TextBlockFormation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>> textLines) {
	mImg = img;
	mTextLines = textLines;

	mConfig = QSharedPointer<TextBlockFormationConfig>::create();
	mConfig->loadSettings();
}

bool TextBlockFormation::isEmpty() const{
	if (!mImg.empty() && !mTextLines.isEmpty())
		return false;
	else
		return true;
}

bool TextBlockFormation::compute() {

	if (mTextLines.isEmpty())
		return false;

	//sort text lines according to y_max coordinate for more efficient computation
	std::sort(mTextLines.begin(), mTextLines.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->boundingBox().top() < rhs->boundingBox().top();
	});

	PixelSet pgSet;
	for (auto tl : mTextLines) {
		QSharedPointer<Pixel> tlPixel = tl->convertToPixel();
		pgSet.add(tlPixel);
		lineLookUp.insert(tlPixel->id(), tl);
	}
	
	PixelGraph  pg = computeTextLineGraph(pgSet);

	computeTextBlocks(pg);
	//cv::Mat imgTb = draw(mImg);

	refineTextBlocks();
	//cv::Mat imgSplitTB = draw(mImg);

	return true;
}

QSharedPointer<TextBlockFormationConfig> TextBlockFormation::config() const {
	return qSharedPointerDynamicCast<TextBlockFormationConfig>(mConfig);
}

PixelGraph TextBlockFormation::computeTextLineGraph(PixelSet pgSet) {
	// compute pixel graph for text line regions
	
	TLConnector tlpc = TLConnector(mImg);
	tlpc.setStopLines(mSeparatorLines);

	PixelGraph pg(pgSet);
	pg.connect(tlpc);

	//debug draw
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);
	//
	//for (auto tl : mTextLines) {
	//	tl->convexHull().draw(painter);
	//	//tl->draw(painter);
	//}
	//
	//pg.draw(painter);
	//cv::Mat results = Image::qImage2Mat(qImg);

	return pg;
}

void TextBlockFormation::computeTextBlocks(PixelGraph pg) {

	computeAdjacency(pg);
	
	//find lines at the beginning of a text block
	QVector<QSharedPointer<WSTextLineSet>> startLines;	
	for (auto tl : mTextLines) {
		QString id = tl->id();
		if (!annCount.contains(id) || annCount.value(id) != 1)
			startLines << tl;

		if (bnnIndices.contains(id) && bnnIndices.value(id).size() > 1) {
			for (int idx : bnnIndices.value(id))
				startLines << mTextLines[idx];
		}		
	}

	//cluster lines together according to their adjacency relations
	for (auto tl : startLines) {
		QVector<QSharedPointer<rdf::TextLineSet>> textBlockLines;
		textBlockLines << qSharedPointerCast<TextLineSet>(tl);
		QString id = tl->id();

		if (bnnIndices.contains(id) &&  bnnIndices.value(id).length() == 1) {
			int nextIdx = bnnIndices.value(id)[0];

			while (true) {

				auto tmp = mTextLines[nextIdx];
				auto tmpId = tmp->id();

				if ((annCount.contains(tmpId) && annCount.value(tmpId) > 1))
					break;

				textBlockLines << tmp;	
	
				if (!bnnIndices.contains(tmpId) || bnnIndices.value(tmpId).length() != 1)
					break;
				
				nextIdx = bnnIndices.value(tmpId)[0];
			}
		}

		mTextBlockSet << createTextBlock(textBlockLines);
	}
}

bool TextBlockFormation::checkInput() const{
	return !isEmpty();
}

void TextBlockFormation::computeAdjacency(PixelGraph pg) {

	//TODO consider using height ratio check after text block formation
		//deleting links to headlines might reduce performance
		//avoid linking with erroneous text lines by using lower threshold in adjacency computation
		//use higher threshold for splitting tb later on
	//TODO fix minHeightRatio parameter and computation of heightRatio value
	
	bool debugDraw = false;
	double minHeightRatio = 0.75;

	for (auto pixel : pg.set().pixels()) {

		auto lineSet1 = lineLookUp.value(pixel->id());
		QVector<int> eIndices = pg.edgeIndexes(pixel->id());

		if (eIndices.isEmpty())
			continue;

		double minDist = DBL_MAX;
		QSharedPointer<WSTextLineSet> nearestNeighbor;
		QVector<QSharedPointer<WSTextLineSet>> neighbors;

		//for each pixel find nearest neighbor having similar size
		for (int idx : eIndices) {

			auto lineSet2 = lineLookUp.value(pg.edges()[idx]->second()->id());

			double mph1 = lineSet1->pixelHeight();
			double mph2 = lineSet2->pixelHeight();
			double heightRatio = std::min(mph1, mph2) / std::max(mph1, mph2);

			// find nearest neighbor text line set having similar height
			if (minHeightRatio < heightRatio) {

				neighbors << lineSet2;
				double dist = computeInterLineDistance(lineSet1, lineSet2);

				if (dist < minDist) {
					minDist = dist;
					nearestNeighbor = lineSet2;
				}
			}
		}

		if (nearestNeighbor == NULL)
			continue;

		//see if there are other neighboring text line sets aligned with the nearest neighbor
		if (neighbors.size() > 1) {

			QVector<QSharedPointer<WSTextLineSet>> neighbors_final;
			neighbors_final << nearestNeighbor;
			Rect nnB = nearestNeighbor->boundingBox();

			//TODO should check overlap with neighbors_final instead of only nearestNeighbor
			for (auto n : neighbors) {

				if (n == nearestNeighbor)
					continue;

				Rect cnB = n->boundingBox();
				double overlap = std::min(nnB.bottom(), cnB.bottom()) - std::max(nnB.top(), cnB.top());
				double relOverlap = overlap / std::max(nnB.height(), cnB.height());

				if (overlap > 0 && relOverlap > 0.5)
					neighbors_final << n;
			}
			neighbors = neighbors_final;
		}

		//store adjacency relations of current pixel
		for (auto n : neighbors) {
			auto v1 = bnnIndices.value(lineSet1->id());
			v1 << mTextLines.indexOf(n);
			bnnIndices.insert(lineSet1->id(), v1);

			if (annCount.contains(n->id()))
				annCount.insert(n->id(), annCount.value(n->id()) + 1);
			else
				annCount.insert(n->id(), 1);
		}
	}

	//debug
	if (debugDraw) {
		//QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);	//blank image
		QImage qImg = Image::mat2QImage(mImg, true);
		QPainter painter(&qImg);

		for (auto tl : mTextLines) {

			QPen dotPen(ColorManager::darkGray());
			dotPen.setWidth(10);

			painter.setPen(ColorManager::darkGray());
			tl->convexHull().draw(painter);

			//QString text = "annC = " + QString::number(annCount.value(tl->id())) + ", bnnC = " + QString::number(bnnIndices.value(tl->id()).size()) + ", pH = " + QString::number(tl->pixelHeight());
			//painter.drawText(tl->boundingBox().toQRect(), Qt::AlignCenter, text);

			//draw BNNs
			auto currBnnIndices = bnnIndices.value(tl->id());
			for (int i : currBnnIndices) {
				painter.setPen(ColorManager::blue());
				Line edge = Line(tl->boundingBox().center(), mTextLines[i]->boundingBox().center());
				edge.setThickness(3.0);
				edge.draw(painter);

				painter.setPen(dotPen);
				mTextLines[i]->boundingBox().center().draw(painter);
				tl->boundingBox().center().draw(painter);

				//painter.setPen(ColorManager::blue());
				//double mph1 = tl->pixelHeight();
				//double mph2 = mTextLines[i]->pixelHeight();
				//double heightRatio = std::min(mph1, mph2) / std::max(mph1, mph2);
				//painter.drawText(edge.center().toQPoint(), QString::number(heightRatio));
			}

			//draw lines rejected due to height ratio
			//QString linePixelID = lineLookUp.key(tl);
			//QVector<int> eIndices = pg.edgeIndexes(linePixelID);
			//linePen.setColor(ColorManager::red());

			////for (auto i : eIndices) {
			////	auto bnn = lineLookUp.value(pg.edges()[i]->second()->id());
			////	if (!currBnnIndices.contains(mTextLines.indexOf(bnn))) {

			////		double mph1 = tl->pixelHeight();
			////		double mph2 = bnn->pixelHeight();
			////		double heightRatio = std::min(mph1, mph2) / std::max(mph1, mph2);

			////		if (minHeightRatio < heightRatio)
			////			continue;

			////		painter.setPen(ColorManager::red());
			////		Line edge = Line(tl->boundingBox().center(), bnn->boundingBox().center());
			////		painter.drawText(edge.center().toQPoint(), QString::number(heightRatio));
			////		edge.draw(painter);

			////		painter.setPen(dotPen);
			////		bnn->boundingBox().center().draw(painter);
			////	 }
			////}
		}

		cv::Mat results = Image::qImage2Mat(qImg);
	}
}

TextBlock TextBlockFormation::createTextBlock(const QVector<QSharedPointer<TextLineSet>>& lines) {

	QVector<QSharedPointer<TextLine>> lines_;
	PixelSet pSet;
	for (auto tl : lines) {
		lines_ << tl->toTextLine();
		pSet.append(tl->pixels());
	}

	QPolygonF poly = computeTextBlockPoly(lines_, config()->polygonType());

	//QPolygonF poly;
	//Rect bbox = Rect();
	//PixelSet pSet;

	//QImage qPolyImg(mImg.size().width, mImg.size().height, QImage::Format_RGB888);
	//qPolyImg.fill(QColor(0, 0, 0));
	//QPainter painter(&qPolyImg);
	//painter.setPen(QColor(255, 255, 255));
	//painter.setBrush(QColor(255, 255, 255));

	//for (int i = 0; i < lines.length(); i++) {

	//	auto l1 = lines[i];
	//	pSet.append(l1->pixels());

	//	if (i == 0) {
	//		poly = l1->convexHull().polygon();
	//		painter.drawPolygon(poly);
	//	}
	//	else if (i > 0) {

	//		auto l2 = l1;
	//		l1 = lines[i - 1];

	//		PixelSet mergedSet;
	//		mergedSet.append(l1->pixels());
	//		mergedSet.append(l2->pixels());
	//		QPolygonF hull_poly = mergedSet.convexHull().polygon();

	//		auto poly1 = l1->convexHull().polygon();
	//		auto poly2 = l2->convexHull().polygon();

	//		//compute rect containing max left and min right bound of the text lines
	//		auto rect1 = poly1.boundingRect();
	//		auto rect2 = poly2.boundingRect();

	//		double top = std::min(rect1.top(), rect2.top());
	//		double left = std::max(rect1.left(), rect2.left());
	//		double bottom = std::max(rect1.bottom(), rect2.bottom());
	//		double right = std::min(rect1.right(), rect2.right());

	//		Rect rect = Rect(left, top, right - left, bottom - top);
	//		QPolygonF rect_poly = Polygon::fromRect(rect).polygon();

	//		auto cut_poly = hull_poly.intersected(rect_poly);

	//		//QPolygonF cut_poly1, cut_poly2;
	//		//cut_poly1 = cut_poly.united(poly1);
	//		//cut_poly2 = cut_poly.united(poly2);
	//		//QPolygonF cut_poly_final = cut_poly1.united(cut_poly2);

	//		////final merged polygon
	//		//poly = poly.united(cut_poly_final);

	//		painter.drawPolygon(poly2);
	//		painter.drawPolygon(cut_poly);
	//	}
	//}

	//cv::Mat polyImg = Image::qImage2Mat(qPolyImg);
	//cvtColor(polyImg, polyImg, cv::COLOR_RGB2GRAY);

	//std::vector<std::vector<cv::Point>> contours;
	//cv::findContours(polyImg, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	////debug
	////cv::Mat contour_img = cv::Mat::zeros(mImg.size().height, mImg.size().width, CV_8UC1);
	////cv::fillPoly(contour_img, contours, 255, 8);

	//if (contours.size() > 1)
	//	qWarning() << "Found more than one contour element for a single text block. Using only the first one.";

	//poly = Polygon::fromCvPoints(contours[0]).polygon();

	TextBlock tb(poly);

	tb.setTextLines(lines);
	tb.addPixels(pSet);

	return tb;
}

QPolygonF TextBlockFormation::computeTextBlockPoly(const QVector<QSharedPointer<TextLine>>& textLines, int polyType) {

	QPolygonF poly;

	if (polyType < 0 || polyType >= PolygonType::poly_end) {
		qWarning() << "Could not determine text block polygon type! Using contour polygon instead!";
		polyType = PolygonType::poly_contour;
	}

	if (polyType == PolygonType::poly_contour) {
		Rect bbox = Rect();
		PixelSet pSet;

		QImage qPolyImg(mImg.size().width, mImg.size().height, QImage::Format_RGB888);
		qPolyImg.fill(QColor(0, 0, 0));
		QPainter painter(&qPolyImg);
		painter.setPen(QColor(255, 255, 255));
		painter.setBrush(QColor(255, 255, 255));

		for (int i = 0; i < textLines.length(); i++) {

			auto l1 = textLines[i];

			if (i == 0) {
				poly = l1->polygon().polygon();
				painter.drawPolygon(poly);
			}
			else if (i > 0) {

				auto l2 = l1;
				l1 = textLines[i - 1];

				//compute convex hull over both lines
				std::vector<cv::Point> pts = l1->polygon().toCvPoints();
				std::vector<cv::Point> pts2 = l2->polygon().toCvPoints();
				pts.insert(pts.end(), pts2.begin(), pts2.end());

				std::vector<cv::Point> cPts;
				cv::convexHull(pts, cPts, true);
				Polygon tmp_poly = Polygon::fromCvPoints(cPts);
				QPolygonF hull_poly = tmp_poly.polygon();

				auto poly1 = l1->polygon().polygon();
				auto poly2 = l2->polygon().polygon();

				//compute rect containing max left and min right bound of the text lines
				auto rect1 = poly1.boundingRect();
				auto rect2 = poly2.boundingRect();

				double top = std::min(rect1.top(), rect2.top());
				double left = std::max(rect1.left(), rect2.left());
				double bottom = std::max(rect1.bottom(), rect2.bottom());
				double right = std::min(rect1.right(), rect2.right());

				Rect rect = Rect(left, top, right - left, bottom - top);
				QPolygonF rect_poly = Polygon::fromRect(rect).polygon();

				auto cut_poly = hull_poly.intersected(rect_poly);

				//QPolygonF cut_poly1, cut_poly2;
				//cut_poly1 = cut_poly.united(poly1);
				//cut_poly2 = cut_poly.united(poly2);
				//QPolygonF cut_poly_final = cut_poly1.united(cut_poly2);

				////final merged polygon
				//poly = poly.united(cut_poly_final);

				painter.drawPolygon(poly2);
				painter.drawPolygon(cut_poly);
			}
		}

		cv::Mat polyImg = Image::qImage2Mat(qPolyImg);
		cvtColor(polyImg, polyImg, cv::COLOR_RGB2GRAY);

		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(polyImg, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

		//debug
		//cv::Mat contour_img = cv::Mat::zeros(mImg.size().height, mImg.size().width, CV_8UC1);
		//cv::fillPoly(contour_img, contours, 255, 8);

		if (contours.size() > 1)
			qWarning() << "Found more than one contour element for a single text block. Using only the first one.";

		poly = Polygon::fromCvPoints(contours[0]).polygon();
	}
	else if (polyType == PolygonType::poly_convex) {

		QVector<Vector2D> pts;
		for (auto tl : textLines)
			pts << tl->polygon().toPoints();
		
		std::vector<cv::Point> ptsCV;
		for (auto p : pts)
			ptsCV.push_back(p.toCvPoint());

		std::vector<cv::Point> cPts;
		cv::convexHull(ptsCV, cPts, true);
		Polygon tmp_poly = Polygon::fromCvPoints(cPts);
		poly = tmp_poly.polygon();
	}
	else if (polyType == PolygonType::poly_bb) {

		QVector<Vector2D> pts;
		for (auto tl : textLines)
			pts << tl->polygon().toPoints();

		Rect box = Rect::fromPoints(pts);
		poly = Polygon::fromRect(box).polygon();
	}

	//clip polygon to image boundaries
	Rect imgRect = Rect(0, 0, mImg.cols-1, mImg.rows-1);
	QPolygonF imgPoly = Polygon::fromRect(imgRect).polygon();
	poly = poly.intersected(imgPoly);

	return poly;
}

void TextBlockFormation::refineTextBlocks() {

	//refine text blocks
		//merge neighboring lines that split paragraphs (l1 -> l2_1 - l2_2 -> l3)

	double lineDistExtFactor = 1.2;

	//merge/ignore little lines >75% covered by other text blocks
	QVector<QSharedPointer<TextBlock>> filtered;
	for (auto tb : mTextBlockSet.textBlocks()) {

		if (filtered.contains(tb))
			continue;

		auto poly = tb->poly().polygon();
		
		for (auto tb2 : mTextBlockSet.textBlocks()) {
			
			if (tb ==  tb2 || filtered.contains(tb2))
				continue;

			auto poly2 = tb2->poly().polygon();

			if (poly.intersects(poly2)) {
					
				cv::Mat polyImg1 = cv::Mat::zeros(mImg.size(), CV_8UC1);
				cv::Mat polyImg2 = cv::Mat::zeros(mImg.size(), CV_8UC1);
				std::vector<cv::Point> pts = tb->poly().toCvPoints();
				cv::fillConvexPoly(polyImg1, pts, cv::Scalar(255));

				std::vector<cv::Point> pts2 = tb2->poly().toCvPoints();
				cv::fillConvexPoly(polyImg2, pts2, cv::Scalar(255));

				cv::Mat mask = polyImg1.mul(polyImg2!=0);
				int tb2Size = cv::countNonZero(polyImg2);
				int overlapSize = cv::countNonZero(mask);
				double overlapRatio = (double)overlapSize / (double)tb2Size;

				if (overlapRatio > 0.75) {
					filtered << tb2;
				}
			}
		}
	}

	for (auto tb : filtered) {
		mTextBlockSet.remove(tb);
	}

	////debug
	//QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);	//blank image
	////QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);
	//painter.setPen(ColorManager::blue());
	//for (auto tb : mTextBlockSet.textBlocks())
	//	tb->draw(painter);

	//painter.setPen(ColorManager::red());
	//for (auto tb : filtered)
	//	tb->draw(painter);

	//cv::Mat results = Image::qImage2Mat(qImg);

	//if (!filtered.isEmpty()) {
	//	qDebug() << "Debug this.";
	//}
	//debugEND

	qInfo() << "Removed " << filtered.size() << " text blocks because they are contained in another one.";

	////debug
	////QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);	//blank image
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);
	//painter.setPen(ColorManager::blue());
	

	QVector<TextBlock> newBlocks;
	filtered.clear();
	
	//split text block if there are gaps that represent inter-paragraph distances or between headlines and paragraphs	
	for(auto tb : mTextBlockSet.textBlocks()){

		auto lines = tb->textLines();
		if (lines.isEmpty() || lines.length() < 4)
			continue;

		//check if link is valid by computing distance
		QMap<int, double> distanceMap;
		for (int i = 1; i < lines.length(); ++i) {
			auto line1 = qSharedPointerCast<WSTextLineSet>(lines[i - 1]);
			auto line2 = qSharedPointerCast<WSTextLineSet>(lines[i]);

			double avgLineDist = computeInterLineDistance(line1, line2);

			double distance = avgLineDist;
			distanceMap.insert(i, distance);
		}

		double medianDistance = Algorithms::statMoment(distanceMap.values(), 0.5);
		QVector<int> splitIndices;

		for (int key : distanceMap.keys()) {
			double dist = distanceMap.value(key);
			if (dist > (medianDistance*lineDistExtFactor))
				splitIndices << key;
		}

		if (splitIndices.isEmpty() || splitIndices.size() > (distanceMap.size() / 2.0))
			continue; //do not split text block if majority of lines will be separated
		else
			filtered << tb;

		int startIdx = 0;
		for (int idx : splitIndices) {
		
			int length = idx - startIdx;
			newBlocks << createTextBlock(lines.mid(startIdx, length));

			if (splitIndices.indexOf(idx) == (splitIndices.size()-1) ) {
				length = -1;
				newBlocks << createTextBlock(lines.mid(idx, length));
			}
			else {
				startIdx = idx;
			}
		}

	//	//cv::Mat results = Image::qImage2Mat(qImg);
	//	//qDebug() << "finished refining text block";
	}

	//remove split text blocks and add new ones
	for (auto tb : filtered)
		mTextBlockSet.remove(tb);

	for (auto tb : newBlocks)
		mTextBlockSet << tb;
}

double TextBlockFormation::computeInterLineDistance(const QSharedPointer<WSTextLineSet>& ls1, const QSharedPointer<WSTextLineSet>& ls2) {

	//TODO assert ls1 lies above ls2 and they do not intersect

	Line l1 = ls1->fitLine();
	Line l2 = ls2->fitLine();

	double d1, d2;

	if (l1.p1().x() > l2.p1().x())
		d1 = l2.distance(l1.p1());
	else
		d1 = l1.distance(l2.p1());

	if (l1.p2().x() < l2.p2().x())
		d2 = l2.distance(l1.p2());
	else
		d2 = l1.distance(l2.p2());

	double avgLineDist = (d1 + d2) / 2;

	return avgLineDist;
}

void TextBlockFormation::addSeparatorLines(const QVector<Line>& lines) {
	mSeparatorLines << lines;
}

cv::Mat TextBlockFormation::draw(const cv::Mat & img, const QColor & col) {

	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	//painter.setPen(ColorManager::darkGray());
	//for (int i = 0; i < mTextLines.size(); ++i) {
	//	auto tl = mTextLines[i];
	//	Polygon tlP = tl->convexHull();
	//	tlP.draw(painter);

	//	//for (int j : bnnIndices[i]) {
	//	//	auto tl2 = mTextLines[j];
	//	//	painter.drawLine(tl->center().toQPoint(), tl2->center().toQPoint());
	//	//}

	//	//QString bC = QString::number(bnnIndices[i].size());
	//	//QString aC = QString::number(annCount[i]);
	//	//painter.drawText(tlR.toQRect(), Qt::AlignCenter, "aC=" + aC + ", bC=" + bC);
	//}

	painter.setPen(ColorManager::blue());
	for (auto tb : mTextBlockSet.textBlocks()) {
		//Rect tbR = Rect::fromPoints(tb->poly().toPoints());
		tb->poly().draw(painter);
	}

	return Image::qImage2Mat(qImg);
}

TextBlockSet TextBlockFormation::textBlockSet() {
	return mTextBlockSet;
}


// Horizontal Pixel Connector --------------------------------------------------------------------

RightNNConnector::RightNNConnector() : PixelConnector() {
}

QVector<QSharedPointer<PixelEdge>> RightNNConnector::connect(const QVector<QSharedPointer<Pixel>>& pixels) const{

	//TODO check parameter choice
	//TODO improve text height estimate computation

	//parameters
	//double maxHeightRatio = 2.5;
	double maxHeightRatio = 3.0;
	double minVertOverlapRatio = 0.30;
	double distMultiplier = 5;
	bool filterEdges = true;		// based on maxHeightRatio + minVertOverlapRatio
	int maxRnnCount = 3;

	QVector<QSharedPointer<PixelEdge> > edges;
	QVector<QSharedPointer<Pixel>> mPixels(pixels);

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(mPixels.begin(), mPixels.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
	});

	QList<double> spacings;
	for (auto p : pixels) {
		spacings << p->bbox().height();
	}

	double medianTextHeight = Algorithms::statMoment(spacings, 0.5);

	//TODO speed up this process by doing undirected linking and computing a RNN look up table
	for (const QSharedPointer<Pixel>& p1 : mPixels) {

		QVector<QSharedPointer<Pixel>> rnn;

		for (const QSharedPointer<Pixel>& p2 : mPixels) {

			if (p1->id() == p2->id())
				continue;

			Rect p1R = p1->bbox();
			Rect p2R = p2->bbox();

			if (p1R.top() > p2R.bottom())
				continue;

			if (p2R.top() > p1R.bottom())
				break;

			if (p2R.right() > p1R.right() && p2R.left() > p1R.left()) {

				//filter pixels according to the x-distance between them
				double xDist = p2R.left() - p1R.right();
				
				if(xDist < medianTextHeight*distMultiplier){
					rnn << p2;
				}
			}
		}
		
		//sort neighboring pixels from left to right (according to center)
		std::sort(rnn.begin(), rnn.end(), [](const auto& lhs, const auto& rhs) {
			return lhs->center().x() < rhs->center().x();
			//return lhs->bbox().left() < rhs->bbox().left();
			//return lhs->bbox().right() < rhs->bbox().right();
		});

		//add edges after filtering according to parameters
		int maxIdx = std::min(rnn.size(), maxRnnCount);
		int newEdgeCount = edges.size()+ maxIdx;
		for (int idx = 0; idx < rnn.size(); idx++) {

			if (edges.size() == newEdgeCount)
				break;

			if (filterEdges) {

				//filter pixel according to x/y overlap
				Rect p1R = p1->bbox();
				Rect p2R = rnn.at(idx)->bbox();

				double heightRatio = std::max(p1R.height(), p2R.height())  / std::min(p1R.height(), p2R.height());

				if(heightRatio < maxHeightRatio){

					//check if relative y-overlap is bigger than 1/3 of bigger component
					double yOverlap = std::min(p1R.bottom(), p2R.bottom()) - std::max(p1R.top(), p2R.top());
					double relYoverlap = yOverlap / std::max(p1R.height(), p2R.height());

					if (yOverlap > 0 && relYoverlap > (minVertOverlapRatio)) {
						edges << QSharedPointer<PixelEdge>::create(p1, rnn.at(idx));	//add edge for remaining pair
					}
				}
			}
			else {
				edges << QSharedPointer<PixelEdge>::create(p1, rnn.at(idx));
			}
		}
	}

	// remove edges that cross stop lines
	filter(edges);

	return edges;
}


// WhiteSpacePixel Connector --------------------------------------------------------------------

WSConnector::WSConnector(){
}

void WSConnector::setLineSpacing(double lineSpacing) {
	mLineSpacing = lineSpacing;
}

QVector<QSharedPointer<PixelEdge>> WSConnector::connect(const QVector<QSharedPointer<Pixel>>& pixels) const{
	
	double lineSpacing;

	if (mLineSpacing > 0.0)
		lineSpacing = mLineSpacing;
	else{
		qWarning() << "Line spacing not specified! Estimating approximated value";
		
		QList<double> spacings;
		for (auto p : pixels) {
			spacings << p->bbox().height();
		}

		lineSpacing = Algorithms::statMoment(spacings, 0.5);
	}
	
	QVector<QSharedPointer<PixelEdge> > edges;
	QVector<QSharedPointer<Pixel>> mPixels(pixels);
	
	//TODO for skewed lines sorting according to bbox().top() might give wrong results -> consider differemt sorting criteria

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(mPixels.begin(), mPixels.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
		//return lhs->bbox().bottom() < rhs->bbox().bottom();
	});

	for (int i1 = 0; i1 < mPixels.size(); ++i1) {

		auto p1 = mPixels[i1];
		Rect p1R = p1->bbox();

		Rect interBox = Rect(p1R.left(), p1R.bottom(), p1R.width(), lineSpacing*2.5);
		QVector<QSharedPointer<Pixel> > neighbors;
		Rect nR;

		for (int i2 = i1+1; i2 < mPixels.size(); ++i2) {

			auto p2 = mPixels[i2];
			Rect p2R = p2->bbox();

			if (p2R.intersects(interBox)) {

				if (neighbors.isEmpty()) {
					neighbors << p2;
					nR = p2R;
					continue;
				}
				
				if (p2R.top() > nR.bottom())
					break;

				for (auto n : neighbors) {
					
					double overlap = std::min(nR.bottom(), p2R.bottom()) - std::max(nR.top(), p2R.top());
					double relOverlap = overlap / std::min(nR.height(), p2R.height());

					if (overlap > 0 && relOverlap > 0.66) {
						neighbors << p2;
						nR = nR.joined(p2R);
						break;
					}
				}

			}
		}

		for (auto n : neighbors) {
			edges << QSharedPointer<PixelEdge>::create(p1, n);
			edges << QSharedPointer<PixelEdge>::create(n, p1);	//bidirectional edges
		}
	}

	// remove edges that cross stop lines
	filter(edges);

	//qInfo() << "Found #" << edges.size() << " white space edges";
	return edges;
}


// Text Line Connector --------------------------------------------------------------------

TLConnector::TLConnector() {
}

TLConnector::TLConnector(cv::Mat img){
	mImg = img;
}

QVector<QSharedPointer<PixelEdge>> TLConnector::connect(const QVector<QSharedPointer<Pixel>>& pixels) const{
	
	double extFactor = 1.5;	//alternative values: 1-3

	QVector<QSharedPointer<PixelEdge> > edges;
	QVector<QSharedPointer<Pixel>> mPixels(pixels);

	//TODO for skewed lines sorting according to bbox().top() might give wrong results -> consider different sorting criteria

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(mPixels.begin(), mPixels.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
		//return lhs->bbox().bottom() < rhs->bbox().bottom();
	});

	//debug
	//QImage qImg(mImg.size().width, mImg.size().height, QImage::Format_ARGB32);	//blank image
	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);


	for (int i1 = 0; i1 < mPixels.size(); ++i1) {

		auto p1 = mPixels[i1];
		Rect p1R = p1->bbox();
		Ellipse p1E = p1->ellipse();
		//Vector2D e1 = p1E.getPoint(0);
		//Vector2D e2 = p1E.getPoint(CV_PI*0.5);
		//Vector2D e3 = p1E.getPoint(CV_PI);
		//Vector2D e4 = p1E.getPoint(CV_PI*1.5);
		//pts << e.getPoint(CV_PI*0.75);

		//painter.setPen(ColorManager::blue());
		//p1R.draw(painter);
		//p1E.draw(painter);

		//int ss = 4;
		//painter.drawEllipse(e1.toQPointF(), ss, ss);
		//painter.drawEllipse(e2.toQPointF(), ss, ss);
		//painter.drawEllipse(e3.toQPointF(), ss, ss);
		//painter.drawEllipse(e4.toQPointF(), ss, ss);

		Rect interBox = Rect(p1R.left(), p1R.bottom(), p1R.width(), (p1E.minorAxis()*2)*extFactor);
		interBox.joined(p1R);
		
		//painter.setPen(ColorManager::red());
		//interBox.draw(painter);
		//cv::Mat resuts = Image::qImage2Mat(qImg);

		for (int i2 = i1 + 1; i2 < mPixels.size(); ++i2) {

			auto p2 = mPixels[i2];
			Rect p2R = p2->bbox();

			if (p2R.intersects(interBox)) {

				if (p2R.top() > interBox.bottom())
					break;

				edges << QSharedPointer<PixelEdge>::create(p1, p2);
			}
		}
	}

	// remove edges that cross stop lines
	filter(edges);

	return edges;
}


// WhiteSpaceRun --------------------------------------------------------------------

WhiteSpaceRun::WhiteSpaceRun(){
}

WhiteSpaceRun::WhiteSpaceRun(QVector<QSharedPointer<WhiteSpacePixel>> wsSet){
	mWhiteSpaces = wsSet;
}

QVector<QSharedPointer<WhiteSpacePixel>> WhiteSpaceRun::whiteSpaces(){
	return mWhiteSpaces;
}

void WhiteSpaceRun::append(const QVector<QSharedPointer<WhiteSpacePixel>>& wsSet){
	mWhiteSpaces << wsSet;
}

void WhiteSpaceRun::add(const QSharedPointer<WhiteSpacePixel>& ws){
	mWhiteSpaces << ws;
}

bool WhiteSpaceRun::contains(const QSharedPointer<WhiteSpacePixel>& ws) const{
	return mWhiteSpaces.contains(ws);
}

void WhiteSpaceRun::remove(const QSharedPointer<WhiteSpacePixel>& ws){
	mWhiteSpaces.remove(mWhiteSpaces.indexOf(ws));
}

int WhiteSpaceRun::size() const{
	return mWhiteSpaces.size();
}

Rect WhiteSpaceRun::boundingBox() const{
	
	if (mWhiteSpaces.isEmpty())
		return Rect();

	if (mWhiteSpaces.size() == 1)
		return mWhiteSpaces[0]->bbox();
	
	Rect bbox = mWhiteSpaces[0]->bbox();
	for (int i = 1; i < mWhiteSpaces.size(); ++i) {
		bbox = bbox.joined(mWhiteSpaces[i]->bbox());
	}

	return bbox;
}

//shifting zero frequency components to center of image, using code from opencv phase correlation function
//https://github.com/opencv/opencv/blob/2da96be217ab437de854aca7f7670c2048fb554b/modules/imgproc/src/phasecorr.cpp
void WSAHelper::fftShift(cv::Mat out) {

	if (out.rows == 1 && out.cols == 1)
		return;

	std::vector<cv::Mat> planes;
	split(out, planes);

	int xMid = out.cols >> 1;
	int yMid = out.rows >> 1;

	bool is_1d = xMid == 0 || yMid == 0;

	if (is_1d) {
		int is_odd = (xMid > 0 && out.cols % 2 == 1) || (yMid > 0 && out.rows % 2 == 1);
		xMid = xMid + yMid;

		for (size_t i = 0; i < planes.size(); i++) {
			cv::Mat tmp;
			cv::Mat half0(planes[i], cv::Rect(0, 0, xMid + is_odd, 1));
			cv::Mat half1(planes[i], cv::Rect(xMid + is_odd, 0, xMid, 1));

			half0.copyTo(tmp);
			half1.copyTo(planes[i](cv::Rect(0, 0, xMid, 1)));
			tmp.copyTo(planes[i](cv::Rect(xMid, 0, xMid + is_odd, 1)));
		}
	}
	else {
		int isXodd = out.cols % 2 == 1;
		int isYodd = out.rows % 2 == 1;
		for (size_t i = 0; i < planes.size(); i++) {
			// perform quadrant swaps...
			cv::Mat q0(planes[i], cv::Rect(0, 0, xMid + isXodd, yMid + isYodd));
			cv::Mat q1(planes[i], cv::Rect(xMid + isXodd, 0, xMid, yMid + isYodd));
			cv::Mat q2(planes[i], cv::Rect(0, yMid + isYodd, xMid + isXodd, yMid));
			cv::Mat q3(planes[i], cv::Rect(xMid + isXodd, yMid + isYodd, xMid, yMid));

			if (!(isXodd || isYodd)) {
				cv::Mat tmp;
				q0.copyTo(tmp);
				q3.copyTo(q0);
				tmp.copyTo(q3);

				q1.copyTo(tmp);
				q2.copyTo(q1);
				tmp.copyTo(q2);
			}
			else {
				cv::Mat tmp0, tmp1, tmp2, tmp3;
				q0.copyTo(tmp0);
				q1.copyTo(tmp1);
				q2.copyTo(tmp2);
				q3.copyTo(tmp3);

				tmp0.copyTo(planes[i](cv::Rect(xMid, yMid, xMid + isXodd, yMid + isYodd)));
				tmp3.copyTo(planes[i](cv::Rect(0, 0, xMid, yMid)));

				tmp1.copyTo(planes[i](cv::Rect(0, yMid, xMid, yMid + isYodd)));
				tmp2.copyTo(planes[i](cv::Rect(xMid, 0, xMid + isXodd, yMid)));
			}
		}
	}

	merge(planes, out);
}

}
