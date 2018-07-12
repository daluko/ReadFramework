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
#include "GraphCut.h"
#include "ElementsHelper.h"

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

	void WhiteSpaceAnalysisConfig::setMserMinArea(int mserMinArea) {
		mMserMinArea = mserMinArea;
	}

	int WhiteSpaceAnalysisConfig::mserMinArea() const {
		return ModuleConfig::checkParam(mMserMinArea, 0, INT_MAX, "mserMinArea");
	}

	void WhiteSpaceAnalysisConfig::setMserMaxArea(int mserMaxArea) {
		mMserMaxArea = mserMaxArea;
	}

	int WhiteSpaceAnalysisConfig::mserMaxArea() const {
		return ModuleConfig::checkParam(mMserMaxArea, 0, INT_MAX, "mserMaxArea");
	}

	void WhiteSpaceAnalysisConfig::setMaxImgSide(int maxImgSide) {
		mMaxImgSide = maxImgSide;
	}

	int WhiteSpaceAnalysisConfig::maxImgSide() const {
		return ModuleConfig::checkParam(mMaxImgSide, 0, INT_MAX, "maxImgSide");
	}

	void WhiteSpaceAnalysisConfig::setScaleInput(bool scaleInput) {
		mScaleInput = scaleInput;
	}

	bool WhiteSpaceAnalysisConfig::scaleInput() const {
		return mScaleInput;
	}

	void WhiteSpaceAnalysisConfig::setDebugDraw(bool debugDraw){
		mDebugDraw = debugDraw;
	}

	bool WhiteSpaceAnalysisConfig::debugDraw() const{
		return mDebugDraw;
	}

	void WhiteSpaceAnalysisConfig::setDebugPath(const QString & dp) {
		mDebugPath = dp;
	}

	QString WhiteSpaceAnalysisConfig::debugPath() const {
		return mDebugPath;
	}
	
	void WhiteSpaceAnalysisConfig::load(const QSettings & settings) {

		mNumErosionLayers = settings.value("numErosionLayers", numErosionLayers()).toInt();
		mMserMinArea = settings.value("mserMinArea", mserMinArea()).toInt();
		mMserMaxArea = settings.value("mserMaxArea", mserMaxArea()).toInt();
		mMaxImgSide  = settings.value("maxImgSide", maxImgSide()).toInt();
		mScaleInput  = settings.value("scaleInput", scaleInput()).toBool();
		mDebugDraw   = settings.value("debugDraw", debugDraw()).toBool();
		mDebugPath   = settings.value("debugPath", debugPath()).toString();
	}

	void WhiteSpaceAnalysisConfig::save(QSettings & settings) const {

		settings.setValue("numErosionLayers", numErosionLayers());
		settings.setValue("mserMinArea", mserMinArea());
		settings.setValue("mserMaxArea", mserMaxArea());
		settings.setValue("maxImgSide", maxImgSide());
		settings.setValue("scaleInput", scaleInput());
		settings.setValue("debugDraw", debugDraw());
		settings.setValue("debugPath", debugPath());
	}


// WhiteSpaceAnalysis --------------------------------------------------------------------
WhiteSpaceAnalysis::WhiteSpaceAnalysis(const cv::Mat& img) {
	mImg = img;
	mTextBlockRegions = QSharedPointer<Region>::create();

	mConfig = QSharedPointer<WhiteSpaceAnalysisConfig>::create();
	mConfig->loadSettings();
}

bool WhiteSpaceAnalysis::isEmpty() const {
	return mImg.empty();
}

bool WhiteSpaceAnalysis::compute() {
	//TODO improve initial set of components used for text line formation 
	//TODO use asssert() function to check input parameters and results
	//TODO compute line spacing estimate only one time and for all modules

	qInfo()<< "Computing white space layout analysis...";

	if (!checkInput())
		return false;

	cv::Mat inputImg = mImg;
	Timer dt;

	ScaleFactory& sf  = ScaleFactory::instance();

	if(config()->scaleInput()){
		sf = ScaleFactory::instance();
		sf.config()->setMaxImageSide(config()->maxImgSide());
		sf.init(mImg.size());

		qDebug() << "scale factor dpi: " << ScaleFactory::scaleFactorDpi();
		mImg = ScaleFactory::scaled(inputImg);
	}

	//get super pixels (text components)
	SuperPixel sp = computeSuperPixels(mImg);
	pSet = sp.pixelSet();

	if (pSet.isEmpty()) {
		qInfo() << "No super pixels found. Finished white space analysis";
		return false;
	}

	//TODO find other solution and remove
	//compute stats (needed for line spacing used in clustering/distance computation)
	if (!computeLocalStats(pSet)){
		qWarning() << "Could not compute local stats for super pixels!";
		return false;
	}

	filterRect = filterPixels(pSet);
	
	TextLineHypothisizer tlh(mImg, pSet);
	// TODO add separator computation and use them in segementation process
	//tlh.addSeparatorLines(mStopLines);

	// compute initial text lines
	if (!tlh.compute()) {
		qWarning() << "Could not compute text line hypotheses!";
		return false;
	}

	auto  textLines = tlh.textLineSets();
	qInfo() << "Finished text line hypotheses.";
	qInfo() << "Number of text lines is " << tlh.textLineSets().size();

	mTextLineHypotheses = textLines;
	//cv::Mat imgDebugWhiteSpaces = drawWhiteSpaces(img);	//based on mTextLineHypotheses and computed white spaces

	// compute white space segmentation for estimated text lines
	WhiteSpaceSegmentation wss(mImg, textLines);
	
	if (!wss.compute()) {
		qWarning() << "Could not compute white space segmentation!";
		return false;
	}
	qInfo() << "Finished white space segmentation.";

	//debug 
	//QString imgPath = Utils::createFilePath(config()->debugPath(), "_whiteSpaces");
	//Image::save(wss.drawSplitTextLines(mImg), imgPath);

	//get segmented text lines
	mWSTextLines = wss.textLineSets();
	//cv::Mat imgDebugWSS = wss.drawSplitTextLines(img);

	//compute text block formed by previously detected text lines
	TextBlockFormation tbf(mImg, mWSTextLines);

	if (!tbf.compute()) {
		qWarning() << "White space analysis: Could not compute text blocks!";
		return false;
	}
	
	qInfo() << "Finished text block formation.";
	mTextBlockSet = tbf.textBlockSet();
	
	mInfo << "white space layout analysis computed in" << dt;

	if(config()->debugDraw())
		drawDebugImages(mImg);

	// scale back to original coordinates
	if (config()->scaleInput()) {
		sf = ScaleFactory::instance();
		sf.config()->setMaxImageSide(config()->maxImgSide());
		sf.init(inputImg.size());

		sf.scaleInv(mTextBlockSet);
	}

	mTextBlockRegions = mTextBlockSet.toTextRegion();

	return true;
}

QSharedPointer<WhiteSpaceAnalysisConfig> WhiteSpaceAnalysis::config() const {
	return qSharedPointerDynamicCast<WhiteSpaceAnalysisConfig>(mConfig);
}

SuperPixel WhiteSpaceAnalysis::computeSuperPixels(const cv::Mat & img){

	//TODO FIX PARAMETERS
	//TODO check if preprocessing image in super pixel class should be changed
	int numErosionLayers = config()->numErosionLayers(); //must be > 0
	int mserMinArea = config()->mserMinArea();
	int mserMaxArea = config()->mserMaxArea();

	//Text Spotter params
	//MSER ms(10, (int)(0.00002*mser_img.cols*mser_img.rows), (int)(0.05*mser_img.cols*mser_img.rows), 1, 0.7);

	// compute super pixels
	SuperPixel sp = SuperPixel(img);
	
	//changing sp parameters here
	auto spConfig = sp.config();
	spConfig->setNumErosionLayers(numErosionLayers);
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
	//TODO find smart/adaptive size filtering constraint

	if (set.isEmpty())
		return Rect();

	QVector<QString> removeIDs;

	//filter pixels according to size constraints-------------------------------------

	//QList<double> spacings;
	//for (auto p : set.pixels()) {
	//	spacings << p->bbox().height();
	//}

	//double ls = Algorithms::statMoment(spacings, 0.5);
	////double ls = set.lineSpacing(0.5);
	//double hLimit = 2 * ls;
	//double wLimit = 2 * ls;

	//qInfo() << "median text height = " << QString::number(ls);

	//TODO fix parameters for estimating filter rects
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
	
	double lbw = std::max(0.0, (q50 - (q75 - q25)) / 1.5);
	double ubw = (q50 + (q75 - q25)) * 2.0;

	// compute bounds for heights
	q25 = Algorithms::statMoment(heights, 0.25);
	q50 = Algorithms::statMoment(heights, 0.5);
	q75 = Algorithms::statMoment(heights, 0.75);

	double lbh = std::max(0.0, (q50 - (q75 - q25)) / 1.5);
	double ubh = (q50 + (q75 - q25)) * 2.0;

	Rect lbfR(0, 0, lbw, lbh);
	Rect ubfR(0, 0, ubw, ubh);
	qInfo() << "lower bound filter rect = " << lbfR.toString();
	qInfo() << "upper bound filter rect = " << ubfR.toString();

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

	QVector<QSharedPointer<Pixel>> removedPixels;
	for (auto id : removeIDs) {
		removedPixels1 << set.find(id);
		set.remove(set.find(id));
	}

	removeIDs.clear();

	//filter overlapping pixels---------------------------------------------------------
	
	//sort pixels for more efficient computation
	auto set_ = set.pixels();
	std::sort(set_.begin(), set_.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
	});

	for (auto p1 : set_) {

		if (removeIDs.contains(p1->id()))
			continue;

		for (int i = set_.indexOf(p1)+1; i < set_.size(); i++){
			auto p2 = set_.at(i);

			if (p1 == p2)
				continue;

			if (removeIDs.contains(p2->id()))
				continue;

			if (p1->bbox().top() > p2->bbox().top())
				continue;

			if (p1->bbox().bottom() < p2->bbox().top())
				break;

			if (p1->bbox().contains(p2->bbox())) {
				removeIDs << p1->id();
				break;
			}
		}
	}

	//remove filtered pixels from set
	for (auto id : removeIDs) {
		removedPixels2 << set.find(id);
		set.remove(set.find(id));
	}

	return lsR;
}

QVector<QSharedPointer<TextRegion>> WhiteSpaceAnalysis::textLineRegions() const{

	QVector<QSharedPointer<TextRegion>> mTextLineRegions;
	QVector<QSharedPointer<Region> > textRegions = RegionManager::filter<Region>(mTextBlockRegions, Region::type_text_line);
	
	if (textRegions.isEmpty())
		return mTextLineRegions;

	for (auto tr : textRegions) {
		mTextLineRegions << qSharedPointerCast<TextRegion>(tr);
	}

	return mTextLineRegions;
}

QSharedPointer<Region> WhiteSpaceAnalysis::textBlockRegions() const {
	return mTextBlockRegions;
}

QVector<QSharedPointer<Region>> WhiteSpaceAnalysis::evalTextBlockRegions() const {
	
	QVector<QSharedPointer<Region>> evalRegions;

	for (auto tb : mTextBlockRegions->children()) {
		tb->removeAllChildren();
		tb->setId("cvl-" + tb->id().remove("{").remove("}"));	//try to please aletheia input
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
	//QFileInfo info(config()->debugPath());
	//if (info.isDir()) {
	//	path = info.absoluteFilePath() + "/debug.png";
	//}
	//else {
	//	path = "debug.png";
	//}

	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	//draw pixel set-------------------------------------------------
	painter.setPen(ColorManager::blue());
	pSet.draw(painter, PixelSet::draw_pixels);
	//for (auto p : pSet.pixels()) {
	//	//p->bbox().draw(painter);
	//	qDebug() << "pixel size = " << p->size() ;
	//}
	painter.end();

	QString imgPath = Utils::createFilePath(path, "_pixelSet");
	cv::Mat img_debug = Image::qImage2Mat(qImg);
	Image::save(img_debug, imgPath);

	qImg = Image::mat2QImage(img, true);
	painter.begin(&qImg);

	// draw pixel set and filtered pixels--------------------------------------------------
	imgPath = Utils::createFilePath(path, "_filtered_pixels");
	img_debug = drawFilteredPixels(img);
	Image::save(img_debug, imgPath);

	// draw text lines-------------------------------------------------------
	for (const QSharedPointer<TextLineSet>& tl : mTextLineHypotheses) {

		painter.setPen(ColorManager::randColor());
		tl->draw(painter, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
	}
	
	imgPath = Utils::createFilePath(path, "_line_hypotheses");
	img_debug = Image::qImage2Mat(qImg);
	Image::save(img_debug, imgPath);

	// draw final text regions-------------------------------------------------------
	imgPath = Utils::createFilePath(path, "_result_text_regions");
	img_debug = draw(img);
	Image::save(img_debug, imgPath);
}

cv::Mat WhiteSpaceAnalysis::drawWhiteSpaces(const cv::Mat & img) {

	QImage qImg = Image::mat2QImage(img, true);
	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	QPainter painter(&qImg);

	for (auto tl : mTextLineHypotheses) {
		for (auto ws : tl->whiteSpacePixels()) {
			painter.setPen(ColorManager::lightGray());

			if (ws->isBCR())
				painter.setPen(ColorManager::red());

			ws->bbox().draw(painter);
		}

		painter.setPen(ColorManager::blue());
		for (auto tp : tl->pixels()) {
			tp->bbox().draw(painter);
		}

		painter.setPen(ColorManager::darkGray());
		tl->boundingBox().draw(painter);
	}

	cv::Mat imgDebugWhiteSpaces = Image::qImage2Mat(qImg);

	return imgDebugWhiteSpaces;
}

cv::Mat WhiteSpaceAnalysis::drawFilteredPixels(const cv::Mat & img){
	
	//debug draw
	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	QImage qImg = Image::mat2QImage(mImg, true);
	QPainter painter(&qImg);


	//draw final pixel Set
	painter.setPen(ColorManager::blue());
	for (auto p : pSet.pixels()) {
		//p->bbox().draw(painter);
		p->draw(painter, 0.3, Pixel::DrawFlags() | Pixel::draw_ellipse);
	}

	//draw pixels deleted due to size contstraints
	painter.setPen(ColorManager::darkGray());
	Rect lb = Rect(0, 0, filterRect.left(), filterRect.top());
	Rect ub = Rect(0, 0, filterRect.right(), filterRect.bottom());
	lb.draw(painter);	//filter rects used as size constraint for pixels
	ub.draw(painter);
	painter.drawText(ub.toQRect(), Qt::AlignCenter, "fR");

	painter.setPen(ColorManager::red());
	for (auto p : removedPixels1) {
		//p->bbox().draw(painter);
		//QString wh = QString::number(p->bbox().width()) + ", " + QString::number(p->bbox().height());
		//painter.drawText(p->bbox().topLeft().toQPoint(), wh);
		p->draw(painter, 0.3 ,Pixel::DrawFlags() | Pixel::draw_ellipse);
	}

	//draw pixels deleted due to size contstraints
	painter.setPen(QColor(255, 255, 102, 255));
	for (auto p : removedPixels2) {
		//p->bbox().draw(painter);
		p->draw(painter, 0.3, Pixel::DrawFlags() | Pixel::draw_ellipse);
	}

	cv::Mat img_debug = Image::qImage2Mat(qImg);
	
	return img_debug;
}

QString WhiteSpaceAnalysis::toString() const {
	return Module::toString();
}

bool WhiteSpaceAnalysis::checkInput() const {
	return !isEmpty();
}


// TextLineHypothisizerConfig ------------------------------------------------------------------------------

TextLineHypothisizerConfig::TextLineHypothisizerConfig() : ModuleConfig("Text Line Hypothisizer Module") {
}

QString TextLineHypothisizerConfig::toString() const {
	return ModuleConfig::toString();
}

void TextLineHypothisizerConfig::setMinLineLength(int length) {
	mMinLineLength = length;
}

int TextLineHypothisizerConfig::minLineLength() const {
	return mMinLineLength;
}

void TextLineHypothisizerConfig::setErrorMultiplier(double multiplier) {
	mErrorMultiplier = multiplier;
}

double TextLineHypothisizerConfig::errorMultiplier() const {
	return checkParam(mErrorMultiplier, 0.0, DBL_MAX, "errorMultiplier");
}

QString TextLineHypothisizerConfig::debugPath() const {
	return mDebugPath;
}

void TextLineHypothisizerConfig::load(const QSettings & settings) {
	mMinLineLength = settings.value("minLineLength", mMinLineLength).toInt();
	mErrorMultiplier = settings.value("errorMultiplier", errorMultiplier()).toDouble();
	mDebugPath = settings.value("debugPath", debugPath()).toString();
}

void TextLineHypothisizerConfig::save(QSettings & settings) const {
	settings.setValue("minLineLength", mMinLineLength);
	settings.setValue("errorMultiplier", errorMultiplier());
	settings.setValue("debugPath", debugPath());
}


// TextLineHypothisizer -------------------------------------------------------------------------------------

TextLineHypothisizer::TextLineHypothisizer(const cv::Mat img, const PixelSet& set){
	mSet = set;
	mImg = img;
	mConfig = QSharedPointer<TextLineHypothisizerConfig>::create();
	mConfig->loadSettings();
}

bool TextLineHypothisizer::compute() {
	//TODO fix parameter settings

	int minTextLineSize = config()->minLineLength();

	if (mSet.isEmpty())
		return false;

	//filterDuplicates(mSet);

	RightNNConnector rnnpc;
	rnnpc.setStopLines(mStopLines);

	PixelGraph pg(mSet);
	pg.connect(rnnpc, PixelGraph::sort_edges);

	mPg = pg; //debug drawing, remove later

	mTextLines = clusterTextLines(pg);
	mergeUnstableTextLines(mTextLines);

	//remove short text lines
	//TODO refine and transfer code to own function
	QVector<QSharedPointer<WSTextLineSet>> removeTl;
	for (auto tl : mTextLines) {
		if (tl->size() < minTextLineSize) {
			removeTl << tl;
		}
	}

	for (auto tl : removeTl) {
		mTextLines.remove(mTextLines.indexOf(tl));
	}

	for (auto tl : mTextLines) {
		extractWhiteSpaces(tl);
	}

	////debug draw
	//cv::Mat result_tmp = drawTextLineHypotheses(mImg);
	//cv::Mat result_tmp2 = drawGraphEdges(mImg);
	//
	//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_textLineHypotheses_debug");
	//Image::save(result_tmp, imgPath);

	//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_textLineHypotheses_debug2");
	//Image::save(result_tmp2, imgPath);

	return true;
}

QVector<QSharedPointer<WSTextLineSet>> TextLineHypothisizer::clusterTextLines(const PixelGraph & graph) const{

	QVector<QSharedPointer<WSTextLineSet> > textLines;
	QVector<QSharedPointer<PixelEdge> > forkEdges;
	QMap<QString, QVector<QString>> inEdges;

	for (auto e : graph.edges()) {
		QString key = e->second()->id();
		QVector<QString> value = inEdges.value(key);
		value << e->first()->id();
		inEdges.insert(key, value);
	}

	bool updated = true;
	int idx = 0;

	for (int i = graph.edges().length() - 1; i >= 0; --i) {
		auto e = graph.edges().at(i);

		/*qInfo() << "Line length = " << e->edge().length() << "  -  Line weight = " << e->edgeWeightConst();*/

		double heat = 1.0 - (++idx / (double)graph.edges().size());
		updated = processEdge(e, textLines, heat);

		//Vector2D tl(475, 376);
		//Vector2D br(621, 413);
		//Rect debugWindow = Rect(tl, br - tl);
		//updated = processEdgeDebug(e, textLines, heat);
		//updated = processEdgeDebug(e, textLines, heat, debugWindow);
	}

	return textLines;
}

bool TextLineHypothisizer::processEdge(const QSharedPointer<PixelEdge>& e, QVector<QSharedPointer<WSTextLineSet>>& textLines, double heat) const{

	bool updated = false;

	int psIdx1 = findSetIndex(e->first(), textLines);
	int psIdx2 = findSetIndex(e->second(), textLines);

	// create a new text line
	if (psIdx1 == -1 && psIdx2 == -1) {

		QVector<QSharedPointer<Pixel> > px;
		px << e->first();
		px << e->second();
		textLines << QSharedPointer<WSTextLineSet>::create(px);
		updated = true;
	}
	else if (psIdx1 == psIdx2) {

	}
	// merge one pixel
	else if (psIdx2 == -1) {

		if (addPixel(textLines[psIdx1], e, heat)) {
			textLines[psIdx1]->add(e->second());
			updated = true;
		}
	}
	// merge one pixel
	else if (psIdx1 == -1) {
		if (addPixel(textLines[psIdx2], e, heat)) {
			textLines[psIdx2]->add(e->first());
			updated = true;
		}
	}
	// merge to same text line
	else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2], heat)) {
		textLines[psIdx2]->append(textLines[psIdx1]->pixels());
		textLines.remove(psIdx1);
		updated = true;
	}

	return updated;
}

bool TextLineHypothisizer::processEdgeDebug(const QSharedPointer<PixelEdge>& e, QVector<QSharedPointer<WSTextLineSet>>& textLines, double heat, Rect debugWindow) const {

	bool updated = false;

	int psIdx1 = findSetIndex(e->first(), textLines);
	int psIdx2 = findSetIndex(e->second(), textLines);

	// debug draw ------------------------------------
	QImage qImg = Image::mat2QImage(mImg);
	QPainter painter(&qImg);

	if(debugWindow.isNull())
		debugWindow = Rect(0, 0, mImg.cols, mImg.rows);

	if (debugWindow.contains(e->first()->bbox()) && debugWindow.contains(e->second()->bbox())) {

		painter.setPen(ColorManager::white(1.0));
		painter.drawRect(debugWindow.toQRect());

		if (psIdx1 == -1) {
			painter.setPen(ColorManager::red(1.0));
			e->first()->draw(painter);
		}
		else {
			painter.setPen(ColorManager::lightGray(1.0));
			textLines[psIdx1]->draw(painter);
			painter.setPen(ColorManager::blue(1.0));
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


		if (psIdx1 != psIdx2 && psIdx1 != -1 && psIdx2 != -1) {
			cv::Mat debugImg = Image::qImage2Mat(qImg);
		}
	}

	// debug draw------------------------------------

	// create a new text line
	if (psIdx1 == -1 && psIdx2 == -1) {

		QVector<QSharedPointer<Pixel> > px;
		px << e->first();
		px << e->second();
		textLines << QSharedPointer<WSTextLineSet>::create(px);
		updated = true;
	}
	else if (psIdx1 == psIdx2) {

	}
	// merge one pixel
	else if (psIdx2 == -1) {

		if (addPixel(textLines[psIdx1], e, heat)) {
			textLines[psIdx1]->add(e->second());
			updated = true;
		}
	}
	// merge one pixel
	else if (psIdx1 == -1) {
		if (addPixel(textLines[psIdx2], e, heat)) {
			textLines[psIdx2]->add(e->first());
			updated = true;
		}
	}
	// merge to same text line
	else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2], heat)) {
		textLines[psIdx2]->append(textLines[psIdx1]->pixels());
		textLines.remove(psIdx1);
		updated = true;
	}


	// debug draw------------------------------------
	if (debugWindow.contains(e->first()->bbox()) && debugWindow.contains(e->second()->bbox())) {

		if (updated)
			painter.setPen(ColorManager::blue(1.0));
		else
			painter.setPen(ColorManager::red(1.0));

		Rect eBox = e->first()->bbox().joined(e->second()->bbox());
		e->draw(painter);

		cv::Mat debugImg = Image::qImage2Mat(qImg);
	}
	// debug draw------------------------------------

	return updated;
}

void TextLineHypothisizer::extractWhiteSpaces(QSharedPointer<WSTextLineSet>& textLine) const {

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
	//for (auto r : mergedRects_final) {
	//	r.draw(p);
	//}
	//p.end();
	//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_mergedRects_final");
	//Image::save(Image::qImage2Mat(qImg), imgPath);
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
	double minBCRSize= textLine->pixelWidth()*0.5;

	//std::vector<float> data(ws_temp.size());
	//for (int i = 0; i < ws_temp.size(); i++) {
	//	data[i] = (float) ws_temp.at(i)->bbox().width(); //placeholder
	//}

	//std::vector<int> labels;
	//std::vector<float> centers;
	//cv::kmeans(data, 2, labels, cv::TermCriteria(CV_TERMCRIT_ITER + CV_TERMCRIT_EPS, 10, 0.1),
	//	3, cv::KMEANS_PP_CENTERS, centers);

	//int minLabel = std::min(centers[0], centers[1]) == centers[0] ? 0 : 1;

	//float maxWSSize = -1;
	//for (int i = 0; i < labels.size(); i++) {
	//	if(labels.at(i)==minLabel){
	//		if (data.at(i) > maxWSSize)
	//			maxWSSize = data.at(i);
	//	}
	//}

	textLine->setMinBCRSize(minBCRSize);
}

int TextLineHypothisizer::findSetIndex(const QSharedPointer<Pixel> &pixel, const QVector<QSharedPointer<WSTextLineSet>> &sets) const{

	assert(pixel);

	if (!sets.empty()) {
		for (int idx = 0; idx < sets.size(); idx++) {
			if (sets[idx]->contains(pixel))
				return idx;
		}
	}

	return -1;
}

bool TextLineHypothisizer::addPixel(QSharedPointer<WSTextLineSet> &set, const QSharedPointer<PixelEdge> &e, double heat) const{

	//TODO add additional check to avoid merging neighboring text lines
	//TODO consider additional condition for max distance between pixels

	double mMinPointDist = 40.0;		// acceptable minimal distance of a point to a line
	double mErrorMultiplier = 1.2;		// maximal increase of error when merging two lines
	double setError = set->error() * config()->errorMultiplier();
	double heatError = mMinPointDist * heat;
	double mErr = std::max(setError, heatError);
	double newErr = set->line().distance(e->second()->center());

	bool hasLowErr =  newErr < mErr;

	double maxHeightRatio = 3.0;
	double minVertOverlapRatio = 0.30;

	double medHeight = set->pixelHeight();

	//debug start

	//qDebug() << "setError: " << QString::number(setError) << ", heatError: " << QString::number(heatError);
	//qDebug() << "mErr: " << QString::number(mErr) << ", newError: " << QString::number(newErr);

	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);

	//painter.setPen(ColorManager::blue());
	//Line l = set->line();

	//l = l.extendBorder(set->boundingBox().joined(e->second()->bbox()));
	//Line l2 = l.moved(Vector2D(0, medHeight));
	//Line l3 = l.moved(Vector2D(0, -medHeight));

	//painter.setPen(ColorManager::darkGray());
	//l.draw(painter);
	//painter.setPen(ColorManager::lightGray());
	//l2.draw(painter);
	//l3.draw(painter);
	//set->draw(painter);

	//painter.setPen(ColorManager::blue());
	//e->first()->draw(painter);
	//painter.setPen(ColorManager::red());
	//e->second()->draw(painter);

	//Rect setBox = set->boundingBox().joined(e->second()->bbox());
	//setBox.expand(20);

	//cv::Mat result = Image::qImage2Mat(qImg);
	//cv::Mat result_detail = result(setBox.toCvRect());

	//debug end

	if (set->line().length() < 10) {
		//qDebug() << "Would add this pixel due to short length of set!";
		return true;
	}

	//if (hasLowErr) {
	//	qDebug() << "Would add this pixel due to small error when adding to set!";
	//	//return true;
	//}

	//if (e->second()->bbox().height() < medHeight*0.3) {
	//	qDebug() << "Would not add this pixel due to small height compared to group!";
	//	//return false;
	//}

	//filter pixel according to x/y overlap
	Rect p1R = e->first()->bbox();
	Rect p2R = e->second()->bbox();

	//check height ratio
	double heightRatio = p1R.height() / p2R.height();

	if ((1 / maxHeightRatio) < heightRatio && heightRatio < maxHeightRatio) {

		//check if relative y-overlap is bigger than 1/3 of bigger component
		double yOverlap = std::min(p1R.bottom(), p2R.bottom()) - std::max(p1R.top(), p2R.top());
		double relYoverlap = yOverlap / std::max(p1R.height(), p2R.height());

		if (yOverlap > 0 && relYoverlap > (minVertOverlapRatio)) {
			return true;
		}
	}
	
	return false;
}

bool TextLineHypothisizer::mergeTextLines(const QSharedPointer<WSTextLineSet>& tls1, const QSharedPointer<WSTextLineSet>& tls2, double heat) const{

	int mMinLineLength = 15;			// minimum text line length when clustering
	double mMinPointDist = 40.0;		// acceptable minimal distance of a point to a line
	double mErrorMultiplier = 1.2;		// maximal increase of error when merging two lines
	double minSetRatio = 0.6;

	double l1Length = tls1->line().length();
	double l2Length = tls2->line().length();

	bool hasMinLength = l1Length > mMinLineLength && l2Length > mMinLineLength;

	double err1 = tls1->error() * mErrorMultiplier;
	double err2 = tls2->error() * mErrorMultiplier;
	double heatErr = mMinPointDist * heat;

	double maxErr1 = std::max(err1, heatErr);
	double minErrTolerance1 = (tls1->pixelHeight()*0.25);	//half of the average pixel height
	maxErr1 = std::max(minErrTolerance1, maxErr1);

	double maxErr2 = std::max(err2, heatErr);
	double minErrTolerance2 = (tls2->pixelHeight()*0.25);	//half of the average pixel height
	maxErr2 = std::max(minErrTolerance2, maxErr2);

	double nErr1 = tls1->computeError(tls2->centers());
	double nErr2 = tls2->computeError(tls1->centers());

	bool err1Low = nErr1 < maxErr1;
	bool err2Low = nErr2 < maxErr2;
	bool hasLowError = err1Low && err2Low;

	//debug

	//qDebug() << "maxErr1: " << QString::number(maxErr1) << ", maxErr2: " << QString::number(maxErr2);
	//qDebug() << "nErr1: " << QString::number(nErr1) << ", nErr2: " << QString::number(nErr2);

	//QImage qImg = Image::mat2QImage(mImg, true);
	//QPainter painter(&qImg);

	//Rect setBox = tls1->boundingBox().joined(tls2->boundingBox());
	//setBox.expand(20);

	//Line l = tls1->line().extendBorder(setBox);
	//Line l2 = tls2->line().extendBorder(setBox);
	//
	//painter.setPen(ColorManager::blue());
	//l.draw(painter);
	//painter.setPen(ColorManager::green());
	//l2.draw(painter);
	//
	//painter.setPen(ColorManager::lightGray());
	//tls1->draw(painter);
	//tls2->draw(painter);

	//cv::Mat result = Image::qImage2Mat(qImg);
	//cv::Mat result_detail = result(setBox.toCvRect());

	//debug end

	// do not merge one and the same textline
	if (tls1 == tls2)
		return false;

	if (!hasMinLength) {
		//qDebug() << "Would merge this due to short length of at least one component";
		return true;
	}

	//if (tls1->boundingBox().intersects(tls2->boundingBox())) {
		
		//TODO handle cases with overlapping areas of text lines
			//forks
			//overlapping due to failed linking, or dangling text elements/symbols

		if (hasLowError)
			return true;
		else {
			if (err1Low || err2Low) {
				double pixNum = tls1->size() + tls2->size();
				if (err1Low) {
					double ratio = tls1->size() / pixNum;
					if ( ratio > minSetRatio)
						return true;
				}
				else if (err2Low) {
					double ratio = tls2->size() / pixNum;
					if (ratio > minSetRatio)
						return true;
				}
			}

			return false;
		}
	//}



	//// do not create vertical lines
	//if (tls1->line().length() < config()->minLineLength() ||
	//	tls2->line().length() < config()->minLineLength())
	//	return true;

	//double maxErr1 = tls1->error() * config()->errorMultiplier();
	//double maxErr2 = tls2->error() * config()->errorMultiplier();

	//double nErr1 = tls1->computeError(tls2->centers());
	//double nErr2 = tls2->computeError(tls1->centers());

	////return nErr1 < maxErr1 && nErr2 < maxErr2;
	//return nErr1 < maxErr1 || nErr2 < maxErr2;

	return true;
}

void TextLineHypothisizer::mergeUnstableTextLines(QVector<QSharedPointer<WSTextLineSet>>& textLines) const {

	//TODO check further text lines (short, unstable) not only overlapping ones

	QVector<QSharedPointer<TextLineSet>> textLines_;

	//// parameter - how much do we extend the text line?
	for (auto tl : textLines) {
		textLines_.append(qSharedPointerCast<TextLineSet>(tl));
	}

	QVector<QSharedPointer<TextLineSet>> unstable = TextLineHelper::filterAngle(textLines_);

	double tlExtFactor = 1.2;

	// cache convex hulls
	QVector<Polygon> polys;
	for (const auto tl : textLines) {
		polys << tl->convexHull();
	}

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

			// do not merge myself
			if (textLines[idx]->id() == utl->id())
				continue;

			// find all candidate textlines
			if (polys[idx].contains(vl) || polys[idx].contains(vr)) {

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

	qInfo() << "Merged " << QString::number(numRemoved) << " unstable text lines.";

	QVector<QSharedPointer<WSTextLineSet>> unstableLines;

	for (auto tl1 : textLines) {
		for (auto tl2 : textLines) {

			if (tl1->id() == tl2->id())
				continue;

			if (tl1->boundingBox().area() > tl2->boundingBox().area()) {
				if (tl1->boundingBox().intersects(tl2->boundingBox())) {
					auto poly = tl1->convexHull();
					int pCount = 0;
					for (auto p : tl2->pixels()) {
						if (poly.contains(p->center()))
							pCount++;
					}
					double relOverlap = pCount / tl2->size();
					if (relOverlap > 0.75) {
						tl1->append(tl2->pixels());
						unstableLines << tl2;
					}
				}
			}
		}
	}

	for (auto ul : unstableLines) {
		int idx = textLines.indexOf(ul);

		if (idx != -1)
			textLines.remove(idx);
	}

	qInfo() << "Merged " << unstableLines.size() << " unstable text lines.";
}

QVector<QSharedPointer<TextLine>> TextLineHypothisizer::textLines() const {

	QVector<QSharedPointer<TextLine>> tls;
	for (auto set : mTextLines)
		tls << set->toTextLine();

	return tls;
}

QVector<QSharedPointer<WSTextLineSet>> TextLineHypothisizer::textLineSets() const {
	return mTextLines;
}

bool TextLineHypothisizer::isEmpty() const{
	return mSet.isEmpty();
}

void TextLineHypothisizer::addSeparatorLines(const QVector<Line>& lines) {
	mStopLines << lines;
}

bool TextLineHypothisizer::checkInput() const {
	return !mSet.isEmpty();
}

QSharedPointer<TextLineHypothisizerConfig> TextLineHypothisizer::config() const {
	return qSharedPointerDynamicCast<TextLineHypothisizerConfig>(mConfig);
}

cv::Mat TextLineHypothisizer::draw(const cv::Mat& img, const QColor& col) {

	QImage qImg = Image::mat2QImage(img, true);

	QPainter p(&qImg);
	p.setPen(col);

	if (!col.isValid())
		p.setPen(ColorManager::randColor());

	// draw text lines
	for (const QSharedPointer<TextLineSet>& tl : mTextLines) {

		p.setPen(ColorManager::randColor());
		
		tl->draw(p, PixelSet::DrawFlags() | PixelSet::draw_poly | PixelSet::draw_pixels /*, Pixel::draw_stats*/);
		//tl->line().extendBorder(Rect(0,0,img.cols, img.rows)).draw(p);

		//Vector2D c = tl->center();
		//c.setX(c.x() + 20);
		//p.drawText(c.toQPointF(), QString::number(tl->density()));
	}

	return Image::qImage2Mat(qImg);
}

cv::Mat TextLineHypothisizer::drawGraphEdges(const cv::Mat& img, const QColor& col) {
	
	QImage qImg = Image::mat2QImage(img, true);
	QPainter p(&qImg);
	p.setPen(col);

	mPg.draw(p);

	return Image::qImage2Mat(qImg);
}

cv::Mat TextLineHypothisizer::drawTextLineHypotheses(const cv::Mat& img) {
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	for (auto tl : mTextLines) {
		painter.setPen(ColorManager::randColor());
		tl->convexHull().draw(painter);

		painter.setPen(ColorManager::randColor());
		for (auto p : tl->pixels()) {
			p->bbox().draw(painter);
		}

		painter.setPen(ColorManager::red());
		for (auto ws : tl->whiteSpacePixels()) {
			ws->bbox().draw(painter);
		}
	}

	return Image::qImage2Mat(qImg);
}

// WhiteSpaceSegmentation --------------------------------------------------------------------

WhiteSpaceSegmentation::WhiteSpaceSegmentation() {
}

WhiteSpaceSegmentation::WhiteSpaceSegmentation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>>& tlsM) {
	mImg = img;
	mTlsM = tlsM;
}

bool WhiteSpaceSegmentation::isEmpty() const {
	return mTlsM.isEmpty();
}

bool WhiteSpaceSegmentation::compute() {

	if (mTlsM.isEmpty()) {
		qInfo("White space segmentation skipped. There are no text line to be processed.");
		return true;
	}

	//split text lines based on white spaces
	if (!splitTextLines()) {
		qInfo("Finished white space segmentation, no splitting of text lines required.");
		return true;
	}

	if (mTlsM.isEmpty() || mBcrM.isEmpty()) {
		qWarning() << "Error: No text lines or white spaces found for segmentation!";
		return false;
	}
	
	cv::Mat initialTextLines = drawSplitTextLines(mImg);

	PixelGraph pg = computeSegmentationGraph();

	//TODO remove bcr with no connections to other bcr 
	// or remove bcr that only have connections to text pixels?
	removeIsolatedBCR(pg);
	processShortTextLines();

	cv::Mat noIsolatedTextLines = drawSplitTextLines(mImg);
	
	if (!findWhiteSpaceRuns(pg)) {
		qInfo("Finished white space segmentation, no white space runs found.");
		return true;
	}

	cv::Mat whiteSpaceRuns = drawWhiteSpaceRuns(mImg);

	bool updatedSegmentation = true;
	while (updatedSegmentation) {

		//update status of remaining BCR
		updateBCRStatus();

		//remove groups that represent non-bcr runs or are too short
		updatedSegmentation = refineWhiteSpaceRuns();
	}

	cv::Mat finalTextLines = drawSplitTextLines(mImg);

	//TODO further refinement of text lines!?

	////debug draw
	//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_whiteSpaceSegmentation_debug");
	//Image::save(drawSplitTextLines(mImg) , imgPath);

	return true;
}

bool WhiteSpaceSegmentation::checkInput() const {
	return !mTlsM.isEmpty();
}

bool WhiteSpaceSegmentation::splitTextLines(){

	QVector<QSharedPointer<WSTextLineSet>> sTls;
	QVector<QSharedPointer<WhiteSpacePixel>> bcrM;	//white space between split text lines

	//split text lines according to white spaces (bcr)	
	for (auto tl : mTlsM) {

		QVector<QSharedPointer<Pixel>> textPixels(tl->pixels());
		QVector<QSharedPointer<WhiteSpacePixel>> wsPixels(tl->whiteSpacePixels());

		if (wsPixels.isEmpty()) {
			sTls << tl;
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
				bcrM << ws;

				for (int j = lastTPidx; textPixels.size(); ++j) {
					if (textPixels[j]->bbox().right() > ws->bbox().left()) {

						QSharedPointer<WSTextLineSet> tls;
						QVector<QSharedPointer<Pixel>> stp = textPixels.mid(lastTPidx, (j - lastTPidx));

						if (i == 0 || i == (wsPixels.size() - 1) || (i - lastWSidx) == 0) { //add white space?
							tls = QSharedPointer<WSTextLineSet>::create(stp);
							tls->setMinBCRSize(tl->minBCRSize());
							sTls << tls;
						}
						else {
							QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx, (i - lastWSidx));
							tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
							tls->setMinBCRSize(tl->minBCRSize());
							sTls << tls;
						}

						//save indices of text line sets neighboring bcr
						if (lastWSidx != 0) {
							QString key = bcrM[bcrM.size() - 2]->id();
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
			sTls << tls;
		}
		else {
			QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx);
			tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
			tls->setMinBCRSize(tl->minBCRSize());
			sTls << tls;
		}

		QString key = bcrM[bcrM.size() - 1]->id();
		QVector<QSharedPointer<WSTextLineSet>> value = mBcrNeighbors.value(key);
		value << tls;
		mBcrNeighbors.insert(key, value);
	}

	if (bcrM.isEmpty()) {
		return false;
	}

	mBcrM = bcrM;
	mTlsM = sTls;

	return true;
}

double WhiteSpaceSegmentation::computeLineSpacing() const{
	//QList<double> spacings;
	//for (auto tl : mTlsM) {
	//	spacings << tl->lineSpacing();
	//}

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
	PixelSet wsSet;
	QVector<QSharedPointer<TextRegionPixel>> trSet;

	//create pixel set for white space computation (text regions + BCR)
	for (auto sl : mTlsM) {
		QSharedPointer<TextRegionPixel> trPixel = sl->convertToPixel();
		trSet << trPixel;
		wsSet.add(qSharedPointerCast<Pixel>(trPixel));
	}

	for (auto bcr : mBcrM) {
		wsSet.add(qSharedPointerCast<Pixel>(bcr));
	}

	//find horizontally overalapping white spaces
	WSConnector wspc;
	wspc.setLineSpacing(computeLineSpacing());

	PixelGraph pg(wsSet);
	pg.connect(wspc);

	return pg;
}

void WhiteSpaceSegmentation::removeIsolatedBCR(PixelGraph pg) {
	
	//TODO set parameter in config;
	double gapExtFactor = 1.3;

	QVector<QSharedPointer<WhiteSpacePixel>> isolatedBCR;

	for (auto bcr : mBcrM) {
		bool isIsolated = true;

		auto edgeIndexes = pg.edgeIndexes(bcr->id());

		if (edgeIndexes.size() == 0) {

			//TODO check if these bcr should be investigated further
			bcr->setBCR(false);
			isolatedBCR << bcr;
			continue;
		}

		for (int idx : edgeIndexes) {

			auto linkedPixel = pg.edges()[idx]->second();
			if (mBcrM.contains(qSharedPointerCast<WhiteSpacePixel>(linkedPixel))) {
				isIsolated = false;
				break;
			}
		}

		if (isIsolated)
			isolatedBCR << bcr;
	}

	//remove isolated bcr (and merge neighboring text lines)
	deleteBCR(isolatedBCR);

	qInfo() << isolatedBCR.size() << " isolated white spaces have been removed";
	//qInfo() << "there are " << mBcrM.size() << " white spaces remaining";
}

void WhiteSpaceSegmentation::processShortTextLines(){
	
	QImage qImg = Image::mat2QImage(mImg, true);
	QPainter painter(&qImg);

	//merge short text lines enclosed by bcrs with nearest text line
	for (auto tls : mTlsM) {
		if (tls->isShort()){
			painter.setPen(ColorManager::blue());
			tls->boundingBox().draw(painter);

			QVector<QSharedPointer<WhiteSpacePixel>> tlsNeighbors;

			for (auto bcr : mBcrM) {
				auto neighbors = mBcrNeighbors.value(bcr->id());

				if (neighbors.contains(tls)) {
					tlsNeighbors.append(bcr);
				}

				if (tlsNeighbors.size() == 2)
					break;
			}

			if (tlsNeighbors.size() == 2) {
				if (tlsNeighbors[0]->bbox().width() < tlsNeighbors[1]->bbox().width()) {
					auto ntl = mBcrNeighbors.value(tlsNeighbors[0]->id());
					ntl[0]->addWhiteSpace(tlsNeighbors[0]);
					ntl[0]->mergeWSTextLineSet(ntl[1]);
				}
				else {
					auto ntl = mBcrNeighbors.value(tlsNeighbors[1]->id());
					ntl[0]->addWhiteSpace(tlsNeighbors[1]);
					ntl[0]->mergeWSTextLineSet(ntl[1]);
				}
			}
		}
	}

	//revoke bcr status for white spaces thar are enclosed by short text lines
	for(auto bcr : mBcrM){
		auto neighbors = mBcrNeighbors.value(bcr->id());
		bool hasOnlyShortNeighbors = true;

		for (auto n : neighbors) {
			if (!n->isShort()) {
				hasOnlyShortNeighbors = false;
			}
		}

		if (hasOnlyShortNeighbors) {
			bcr->setBCR(false);
		}
	}

	cv::Mat result = Image::qImage2Mat(qImg);
}

void WhiteSpaceSegmentation::deleteBCR(const QVector<QSharedPointer<WhiteSpacePixel>>& bcrM) {
	for (auto p : bcrM) {
		deleteBCR(p);
	}
}

void WhiteSpaceSegmentation::deleteBCR(const QSharedPointer<WhiteSpacePixel>& bcr) {

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

				if (e->first()->bbox().top() > e->second()->bbox().bottom()) {
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

				if (fR.bottom() < sR.top()) {

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
				if (maxGapWsr < tl->maxGap()) {
					maxGapWsr = tl->maxGap();
				}
			}
		}

		for (auto ws : wsr->whiteSpaces()) {
			if (ws->bbox().width() < maxGapWsr*gapExtFactor) {
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

		if (wsr->whiteSpaces().size() > 1) {
			double maxGapWsr = 0;
			for (auto ws : wsr->whiteSpaces()) {
				auto ntl = mBcrNeighbors.value(ws->id());
				for (auto tl : ntl) {
					if (maxGapWsr < tl->maxGap()) {
						maxGapWsr = tl->maxGap(); 
					}
				}
			}

			bool isObsolete = true;
			for (auto ws : wsr->whiteSpaces()) {
				if (ws->bbox().width() < maxGapWsr)
					isObsolete = false;
			}
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

QVector<QSharedPointer<WSTextLineSet>> WhiteSpaceSegmentation::textLineSets() const {
	return mTlsM;
}

QVector<QSharedPointer<WhiteSpacePixel>> WhiteSpaceSegmentation::bcrSet() const{
	return mBcrM;
}

cv::Mat WhiteSpaceSegmentation::draw(const cv::Mat & img, const QColor & col) {

	return img;
}

cv::Mat WhiteSpaceSegmentation::drawSplitTextLines(const cv::Mat & img, const QColor & col){
	//QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	painter.setPen(ColorManager::blue());
	for (auto tl : mTlsM) {
		tl->boundingBox().draw(painter);
		QString maxGap = QString::number(tl->maxGap());
		painter.drawText(tl->boundingBox().toQRect(), Qt::AlignCenter, maxGap);
	}

	//painter.setPen(ColorManager::red());
	for (auto bcr : mBcrM) {
		if(bcr->isBCR())
			painter.setPen(ColorManager::red());
		else
			painter.setPen(ColorManager::darkGray());

		bcr->bbox().draw(painter);
		QString wsGap = QString::number(bcr->bbox().width());
		painter.drawText(bcr->bbox().toQRect(), Qt::AlignCenter, wsGap);
	}
	
	return Image::qImage2Mat(qImg);
}

cv::Mat WhiteSpaceSegmentation::drawWhiteSpaceRuns(const cv::Mat & img, const QColor & col){

	//QImage qImg = Image::mat2QImage(img, true);
	QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);
	QPainter painter(&qImg);

	//painter.setPen(ColorManager::green());
	//for (auto e : bcrEdges) {
	//	e->draw(painter);
	//}

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
		wsr->boundingBox().draw(painter);
	}

	return Image::qImage2Mat(qImg);
}


// TextBlockFormation --------------------------------------------------------------------

TextBlockFormation::TextBlockFormation() {
}

TextBlockFormation::TextBlockFormation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>> textLines) {
	mImg = img;
	mTextLines = textLines;
}

bool TextBlockFormation::compute() {

	if (mTextLines.isEmpty())
		return false;

	//sort text lines according to y_max coordinate for more efficient computation
	std::sort(mTextLines.begin(), mTextLines.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->boundingBox().top() < rhs->boundingBox().top();
	});

	computeAdjacency();
	formTextBlocks();

	//TODO split text blocks into paragraphs

	// TODO remove parentheses to please Aletheia and avoid errors
	//tb->setPolygon(rdf::Polygon::fromRect(bb));
	//tb->setId(tb->id().remove("{").remove("}"));	
	//tb->setType(rdf::Region::type_text_region);

	//cv::Mat imgBf = draw(mImg);
	//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_blockFormation");
	//Image::save(imgBf, imgPath);

	return true;
}

void TextBlockFormation::computeAdjacency() {

	//Compute below and above nearest neighbors for tlc
	bnnIndices = QVector<QVector<int>>(mTextLines.length());
	annCount = QVector<int>(mTextLines.length(), 0);

	for (int i = 0; i < mTextLines.length(); ++i) {

		Rect r1 = mTextLines[i]->boundingBox();
		Rect bnn1;

		for (int j = i + 1; j < mTextLines.length(); ++j) {
			Rect r2 = mTextLines[j]->boundingBox();

			//check for x overlap of tlc
			if (r1.left() <= r2.right() && r2.left() <= r1.right()) {

				//TODO check distance between text lines ( < max distance)
				Line li = mTextLines[i]->fitLine();
				Line lj = mTextLines[j]->fitLine();
				
				double d1, d2;

				if (li.p1().x() > lj.p1().x())
					d1 = lj.distance(li.p1());
				else
					d1 = li.distance(lj.p1());

				if (li.p2().x() < lj.p2().x())
					d2 = lj.distance(li.p2());
				else
					d2 = li.distance(lj.p2());

				double avgLineDist = (d1 + d2) / 2;

				double textHeight = std::max(mTextLines[i]->pixelHeight(), mTextLines[j]->pixelHeight());
				double extFactor = 3.0;
				
				if (avgLineDist > (textHeight * extFactor)) {
					break;
				}

				//QImage qImg = Image::mat2QImage(mImg, true);
				//QPainter painter(&qImg);

				//painter.setPen(ColorManager::blue());
				//li.draw(painter);
				//lj.draw(painter);

				//if (li.p1().x() > lj.p1().x()) {
				//	if (li.p1().y() > lj.p1().y()) {
				//		Rect mergeR = Rect(li.p1()- Vector2D(0, textHeight*2.5), Vector2D(textHeight*2.5, textHeight*2.5));
				//		mergeR.draw(painter);
				//	}
				//	else {
				//		Rect mergeR = Rect(li.p1(), Vector2D(textHeight*2.5, textHeight*2.5));
				//		mergeR.draw(painter);
				//	}
				//}
				//else {
				//	if (lj.p1().y() > li.p1().y()) {
				//		Rect mergeR = Rect(lj.p1() - Vector2D(0, textHeight*2.5), Vector2D(textHeight*2.5, textHeight*2.5));
				//		mergeR.draw(painter);
				//	}
				//	else {
				//		Rect mergeR = Rect(lj.p1(), Vector2D(textHeight*2.5, textHeight*2.5));
				//		mergeR.draw(painter);
				//	}
				//}

				//cv::Mat results = Image::qImage2Mat(qImg);

				if (bnn1.isNull()) {
					bnnIndices[i] = QVector<int>(1, j);
					annCount[j] = annCount[j] + 1;
					bnn1 = r2;
				}
				else {
					//check for y overlap with previously found bnn
					double overlap = std::min(bnn1.bottom(), r2.bottom()) - std::max(bnn1.top(), r2.top());
					double relOverlap = overlap / std::min(bnn1.height(), r2.height());
					if (overlap > 0 && relOverlap > 0.5) {
						//if (bnn1.top() <= r2.bottom() && r2.top() <= bnn1.bottom()) {
						bnnIndices[i] << j;
						annCount[j] = annCount[j] + 1;
					}
					else {
						break;
					}
				}
			}
		}
	}
}

void TextBlockFormation::formTextBlocks() {

	//form text blocks by grouping text (line) regions
	for (int i = 0; i < mTextLines.length(); ++i) {

		if (annCount.at(i) != 1) {
			QVector<QSharedPointer<rdf::TextLineSet>> textBlockLines;
			textBlockLines << qSharedPointerCast<TextLineSet>(mTextLines[i]);

			if (bnnIndices.at(i).length() == 1) {
				appendTextLines(i, textBlockLines);
			}

			mTextBlockSet << createTextBlock(textBlockLines);
		}

		if (bnnIndices.at(i).length() > 1) {
			for (int idx : bnnIndices.at(i)) {
				if (annCount[idx] == 1) {
					QVector<QSharedPointer<rdf::TextLineSet>> textBlockLines;
					textBlockLines << qSharedPointerCast<TextLineSet>(mTextLines[idx]);

					if (bnnIndices.at(idx).length() == 1) {
						appendTextLines(idx, textBlockLines);
					}

					mTextBlockSet << createTextBlock(textBlockLines);
				}
			}
		}
	}
}

void TextBlockFormation::appendTextLines(int idx, QVector<QSharedPointer<TextLineSet> >& textLines) {

	int nextIdx = bnnIndices.at(idx)[0];
	while (true) {

		if ((annCount.at(nextIdx) > 1)) {
			break;
		}

		textLines << mTextLines[nextIdx];

		if (bnnIndices.at(nextIdx).length() != 1) {
			break;
		}
		nextIdx = bnnIndices.at(nextIdx)[0];
	}
}

TextBlock TextBlockFormation::createTextBlock(const QVector<QSharedPointer<TextLineSet>>& lines) {
	
	Rect bbox = Rect();
	QPolygonF poly;
	PixelSet pSet;

	QImage qPolyImg (mImg.size().width, mImg.size().height, QImage::Format_RGB888);
	qPolyImg.fill(QColor(0, 0, 0));
	QPainter painter(&qPolyImg);
	painter.setPen(QColor(255, 255, 255));
	painter.setBrush(QColor(255, 255, 255));
	
	//debug
	cv::Mat  result3;
	
	for(int i = 0; i < lines.length(); i++){

		auto l1 = lines[i];
		pSet.append(l1->pixels());

		if (i == 0) {
			poly = l1->convexHull().polygon();
			painter.drawPolygon(poly);
		}
		else if (i > 0) {
			
			auto l2 = l1;
			l1 = lines[i-1];

			PixelSet mergedSet;
			mergedSet.append(l1->pixels());
			mergedSet.append(l2->pixels());
			QPolygonF hull_poly = mergedSet.convexHull().polygon();

			auto poly1 = l1->convexHull().polygon();
			auto poly2 = l2->convexHull().polygon();

			//compute rect containing min left and right bound the text lines
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

	//poly = pSet.convexHull();
	TextBlock tb(poly);

	tb.setTextLines(lines);
	tb.addPixels(pSet);

	return tb;
}

cv::Mat TextBlockFormation::draw(const cv::Mat & img, const QColor & col) {

	QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	//QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	painter.setPen(ColorManager::blue());
	for (auto tb : mTextBlockSet.textBlocks()) {
		Rect tbR = Rect::fromPoints(tb->poly().toPoints());
		tbR.draw(painter);
	}

	painter.setPen(ColorManager::darkGray());
	for (int i = 0; i < mTextLines.size(); ++i) {
		auto tl = mTextLines[i];
		Rect tlR = tl->boundingBox();
		tlR.draw(painter);

		QString bC = QString::number(bnnIndices[i].size());
		QString aC = QString::number(annCount[i]);
		painter.drawText(tlR.toQRect(), Qt::AlignCenter, "aC=" + aC + ", bC=" + bC);
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
	double minVertOverlapRatio = 0.33;
	double distMultiplier = 5;
	double textHeightEstimate = 0;	// individual value computed below
	bool filterEdges = false;		// based on maxHeightRatio + minVertOverlapRatio
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

	textHeightEstimate = Algorithms::statMoment(spacings, 0.5);


	for (const QSharedPointer<Pixel>& p1 : mPixels) {

		QVector<QSharedPointer<Pixel>> rnn;

		for (const QSharedPointer<Pixel>& p2 : mPixels) {

			if (p1->id() == p2->id())
				continue;

			if (p1->bbox().top() > p2->bbox().bottom())
				continue;

			if (p2->bbox().top() > p1->bbox().bottom())
				break;
			
			Rect p1R = p1->bbox();
			Rect p2R = p2->bbox();

			if (p2R.right() > p1R.right() && p2R.left() > p1R.left()) {

				//filter pixels according to the x-distance between them
				double xDist = p2R.left() - p1R.right();
				
				if(xDist < textHeightEstimate*distMultiplier){
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

				//check height ratio
				double heightRatio = p1R.height() / p2R.height();

				if ((1 / maxHeightRatio) < heightRatio && heightRatio < maxHeightRatio) {

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

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(mPixels.begin(), mPixels.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
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
					//if (n->bbox().top() <= p2R.bottom() && p2R.top() <= n->bbox().bottom()) {
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

	//qInfo() << "Found #" << edges.size() << " white space edges";
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

}