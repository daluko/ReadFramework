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

	if(config()->scaleInput()){
		int maxImgSide = config()->maxImgSide();
		ScaleFactory& sf = ScaleFactory::instance();
		sf.config()->setMaxImageSide(maxImgSide);
		sf.init(mImg.size());

		qDebug() << "scale factor dpi: " << ScaleFactory::scaleFactorDpi();
		mImg = ScaleFactory::scaled(inputImg);
	}

	SuperPixel sp = computeSuperPixels(mImg);

	//graph based superpixel text line clustering------------------------------------------------------

	//get pixel set
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
	qInfo() << "Computed text line hypotheses.";
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

	if(config()->debugDraw()){
		drawDebugImages(mImg);
	}
	
	// scale back to original coordinates
	if (config()->scaleInput()) {
		ScaleFactory::scaleInv(mTextBlockSet);
	}
	mTextBlockRegions = mTextBlockSet.toTextRegion();

	return true;
}

QSharedPointer<WhiteSpaceAnalysisConfig> WhiteSpaceAnalysis::config() const {
	return qSharedPointerDynamicCast<WhiteSpaceAnalysisConfig>(mConfig);
}

SuperPixel WhiteSpaceAnalysis::computeSuperPixels(const cv::Mat & img){

	//TODO FIX PARAMETERS
	int numErosionLayers = config()->numErosionLayers(); //must be > 0
	int mserMinArea = config()->mserMinArea();
	int mserMaxArea = config()->mserMaxArea();

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
	QList<double> heights;
	QList<double> widths;
	for (auto p : set.pixels()) {
		widths << p->bbox().width();
		heights << p->bbox().height();
	}

	double q25 = Algorithms::statMoment(widths, 0.1);
	double q50 = Algorithms::statMoment(widths, 0.5);
	double q75 = Algorithms::statMoment(widths, 0.9);

	// compute bounds for widths
	double lbw = std::max(0.0, q50 - (q75 - q25));
	double ubw = q50 + (q75 - q25);

	q25 = Algorithms::statMoment(heights, 0.1);
	q50 = Algorithms::statMoment(heights, 0.5);
	q75 = Algorithms::statMoment(heights, 0.9);

	// compute bounds for heights
	double lbh = std::max(0.0, q50 - (q75 - q25));
	double ubh = q50 + (q75 - q25);

	Rect lqfR(0, 0, lbw, lbh);
	Rect uqfR(0, 0, ubw, ubh);
	qInfo() << "lower quartile filter rect = " << lqfR.toString();
	qInfo() << "upper quartile filter rect = " << uqfR.toString();

	QList<double> spacings;
	for (auto p : set.pixels()) {
		spacings << p->bbox().height();
	}

	double ls = Algorithms::statMoment(spacings, 0.5);
	//double ls = set.lineSpacing(0.5);
	double hLimit = 2 * ls;
	double wLimit = 2 * ls;

	Rect lsR(0, 0, wLimit, hLimit);
	qInfo() << "average height rect = " << lsR.toString();
	for (auto p : set.pixels()) {
		//if (p->bbox().height() > hLimit || p->bbox().width() > wLimit) {
		//	removeIDs << p->id();
		//}

		if (p->bbox().height() < lbh || p->bbox().width() < lbw) {
			removeIDs << p->id();
			continue;
		}

		if (p->bbox().height() > ubh || p->bbox().width() > ubw) {
			removeIDs << p->id();
		}
	}

	//remove filtered pixels from pixel set
	QVector<QSharedPointer<Pixel>> removedPixels;
	for (auto id : removeIDs) {
		removedPixels1 << set.find(id);
		set.remove(set.find(id));
	}

	removeIDs.clear();

	//filter overlapping pixels---------------------------------------------------------
	for (auto p1 : set.pixels()) {

		if (removeIDs.contains(p1->id()))
			continue;

		for (auto p2 : set.pixels()) {

			if (p1 == p2)
				continue;

			if (removeIDs.contains(p2->id()))
				continue;

			if (p1->bbox().contains(p2->bbox())) {
				removeIDs << p1->id();
				break;
			}
		}
	}

	//remove filtered pixels from pixel set
	for (auto id : removeIDs) {
		removedPixels2 << set.find(id);
		set.remove(set.find(id));
	}

	return lsR;
}

QVector<QSharedPointer<TextRegion>> WhiteSpaceAnalysis::textLineRegions() const{

	QVector<QSharedPointer<TextRegion>>mTextLineRegions;

	QVector<QSharedPointer<Region> > textRegions = RegionManager::filter<Region>(mTextBlockRegions, Region::type_text_line);
	for (auto tr : textRegions) {
		mTextLineRegions << qSharedPointerCast<TextRegion>(tr);
	}

	return mTextLineRegions;
}

QSharedPointer<Region> WhiteSpaceAnalysis::textBlockRegions() const {
	return mTextBlockRegions;
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

	QString path;
	QFileInfo info(config()->debugPath());
	if (info.isDir()) {
		path = info.absoluteFilePath() + "/debug.png";
	}
	else {
		path = "debug.png";
	}

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
	filterRect.draw(painter);	//filter rect used as size constraint for pixels
	painter.drawText(filterRect.toQRect(), Qt::AlignCenter, "fR");

	painter.setPen(ColorManager::red());
	for (auto p : removedPixels1) {
		//p->bbox().draw(painter);
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

QVector<QSharedPointer<TextLine>> TextLineHypothisizer::textLines() const {

	QVector<QSharedPointer<TextLine>> tls;
	for (auto set : mTextLines)
		tls << set->toTextLine();

	return tls;
}

QVector<QSharedPointer<WSTextLineSet>> TextLineHypothisizer::textLineSets() const {
	return mTextLines;
}

void TextLineHypothisizer::addSeparatorLines(const QVector<Line>& lines) {
	mStopLines << lines;
}

bool TextLineHypothisizer::checkInput() const{
	return !mSet.isEmpty();
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

	for (int i = graph.edges().length() - 1; i >= 0; --i) {
		auto e = graph.edges().at(i);

		if (inEdges.value(e->second()->id()).size() > 1) {
			forkEdges << e;
			continue;
		}

		/*qInfo() << "Line length = " << e->edge().length() << "  -  Line weight = " << e->edgeWeightConst();*/
		updated = processEdge(e, textLines);
	}

	//process pixels representing forks
	for (auto p : graph.set().pixels()) {

		if (inEdges.value(p->id()).size() > 1) {	// inEdges > 1 -> fork

			int mergeIdx = -1;
			int maxSize = -1;

			//find longest set connected to fork (best match for merging)
			for (auto pID : inEdges.value(p->id())) {
				auto first = graph.set().find(pID);
				int tmpIdx = findSetIndex(first, textLines);

				if (tmpIdx != -1) {
					int psSize = textLines[tmpIdx]->size();

					if (psSize > maxSize) {
						maxSize = psSize;
						mergeIdx = tmpIdx;
					}
				}
			}
				
			if (mergeIdx == -1) {
				qWarning() << "Failed to cluster text regions in TextLineHypothisizer.";
				continue;
			}

			int forkSetIdx = findSetIndex(p, textLines);
			if (forkSetIdx == -1) {	//TODO handle cases where idx = -1 (pixel not in a set)
				textLines[mergeIdx]->add(p);
			}
			else {
				textLines[mergeIdx]->append(textLines[forkSetIdx]->pixels());
				textLines.remove(forkSetIdx);
			}
		}
	}

		//for (auto e : forkEdges) {
		//	
		//	int psIdx1 = findSetIndex(e->first(), textLines);
		//	int psIdx2 = findSetIndex(e->second(), textLines);

		//	// create a new text line
		//	if (psIdx1 == psIdx1) {
		//		if (psIdx1 == -1) {
		//			QVector<QSharedPointer<Pixel> > px;
		//			px << e->first();
		//			px << e->second();
		//			textLines << QSharedPointer<TextLineSet>::create(px);
		//			updated = true;
		//		}
		//	}
		//	else if (psIdx2 == -1) {
		//		double maxErr = textLines[psIdx1]->error() * config()->errorMultiplier();
		//		double nErr = textLines[psIdx1]->line().distance(e->second()->center());

		//		if (nErr < maxErr)
		//			textLines[psIdx1]->add(e->second());
		//		else {
		//			QSharedPointer<TextLineSet> tls = QSharedPointer<TextLineSet>::create();
		//			tls->add(e->second());
		//			textLines << tls;
		//		}
		//	}
		//	// merge one pixel
		//	else if (psIdx1 == -1) {
		//		double maxErr = textLines[psIdx2]->error() * config()->errorMultiplier();
		//		double nErr = textLines[psIdx1]->line().distance(e->first()->center());
		//			
		//		if(nErr < maxErr)
		//			textLines[psIdx2]->add(e->first());
		//		else {
		//			QSharedPointer<TextLineSet> tls = QSharedPointer<TextLineSet>::create();
		//			tls->add(e->first());
		//			textLines << tls;
		//		}
		//	}
		//	// merge to same text line
		//	else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2])) {

		//		//textLines[psIdx2]->append(textLines[psIdx1]->pixels());
		//		//textLines.remove(psIdx1);
		//		//updated = true;

		//		//QSharedPointer<TextLineSet> tls1 = textLines[psIdx1];
		//		//QSharedPointer<TextLineSet> tls2 = textLines[psIdx2];

		//		//// do not create vertical lines
		//		//if (tls1->line().length() < config()->minLineLength() ||
		//		//	tls2->line().length() < config()->minLineLength())
		//		//	return true;

		//		//double maxErr1 = tls1->error() * config()->errorMultiplier();
		//		//double maxErr2 = tls2->error() * config()->errorMultiplier();

		//		//double nErr1 = tls1->computeError(tls2->centers());
		//		//double nErr2 = tls2->computeError(tls1->centers());

		//		////return nErr1 < maxErr1 && nErr2 < maxErr2;
		//		//return nErr1 < maxErr1 || nErr2 < maxErr2;
		//	}
		//}

	return textLines;
}

bool TextLineHypothisizer::processEdge(const QSharedPointer<PixelEdge>& e, QVector<QSharedPointer<WSTextLineSet>>& textLines) const{

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

		if (addPixel(textLines[psIdx1], e->second())) {
			textLines[psIdx1]->add(e->second());
			updated = true;
		}
	}
	// merge one pixel
	else if (psIdx1 == -1) {
		if (addPixel(textLines[psIdx2], e->first())) {
			textLines[psIdx2]->add(e->first());
			updated = true;
		}
	}
	// merge to same text line
	else if (mergeTextLines(textLines[psIdx1], textLines[psIdx2])) {

		textLines[psIdx2]->append(textLines[psIdx1]->pixels());
		textLines.remove(psIdx1);
		updated = true;
	}

	return updated;
}

void TextLineHypothisizer::mergeUnstableTextLines(QVector<QSharedPointer<WSTextLineSet>>& textLines) const{

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
		textLines.remove(idx);
	}

	qInfo() << "Merged " << unstableLines.size() << " unstable text lines.";
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
}

int TextLineHypothisizer::findSetIndex(const QSharedPointer<Pixel>& pixel, const QVector<QSharedPointer<WSTextLineSet>>& sets) const{

	assert(pixel);

	if (!sets.empty()) {
		for (int idx = 0; idx < sets.size(); idx++) {
			if (sets[idx]->contains(pixel))
				return idx;
		}
	}

	return -1;
}

bool TextLineHypothisizer::addPixel(QSharedPointer<WSTextLineSet>& set, const QSharedPointer<Pixel>& pixel) const{

	//TODO add additional check to avoid merging neighboring text lines
	//TODO consider additional condition for max distance between pixels
	//find estimate of line spacing or average text component height
	//can be used to limit search range or imply additional condition for connnecting pixels

	//Rect p1R = p1->bbox();
	//Rect p2R = p2->bbox();

	////check height ratio
	//double hRatio = p1R.height() / p2R.height();

	//if (0.4 < hRatio && hRatio < 2.5) {

	//	//check if relative y-overlap is bigger than 1/3 of bigger component
	//	double yOverlap = std::min(p1R.bottom(), p2R.bottom()) - std::max(p1R.top(), p2R.top());
	//	double relYoverlap = yOverlap / std::max(p1R.height(), p2R.height());

	//	if (yOverlap > 0 && relYoverlap > (0.33)) {
	//		return true;
	//	}
	//}


	//if (set->line().length() < 10)
	//	return true;

	//double mErr = set->error() * config()->errorMultiplier();
	//double newErr = set->line().distance(pixel->center());

	//return newErr < mErr;
	
	return true;
}

bool TextLineHypothisizer::mergeTextLines(const QSharedPointer<WSTextLineSet>& tls1, const QSharedPointer<WSTextLineSet>& tls2) const{

	// do not merge one and the same textline
	if (tls1 == tls2)
		return false;

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

bool TextLineHypothisizer::isEmpty() const{
	return mSet.isEmpty();
}

bool TextLineHypothisizer::compute(){
	//TODO find optimal parameter setting and set them using config
	int minTextLineSize = config()->minLineLength();

	if (mSet.isEmpty())
		return false;

	//filterDuplicates(mSet);

	RightNNConnector rnnpc;
	rnnpc.setStopLines(mStopLines);

	PixelGraph pg(mSet);
	pg.connect(rnnpc, PixelGraph::sort_edges);
	
	mPg = pg; //debug drawing, remove later

	clusterTextLines(pg);

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

	//debug draw
	//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_textLineHypotheses_debug");
	//Image::save(drawTextLineHypotheses(mImg), imgPath);

	return true;
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
	QPainter p(&qImg);

	for (auto tl : mTextLines) {
		//tl->convexHull().draw(p);
		
		p.setPen(ColorManager::blue());
		for (auto pix : tl->pixels()) {
			pix->bbox().draw(p);
		}

		p.setPen(ColorManager::red());
		for (auto ws : tl->whiteSpacePixels()) {
			ws->bbox().draw(p);
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
	
	PixelGraph pg = computeSegmentationGraph();

	//TODO remove bcr with no connections to other bcr 
	// or remove bcr that only have connections to text pixels?
	removeIsolatedBCR(pg);

	if (!findWhiteSpaceRuns(pg)) {
		qInfo("Finished white space segmentation, no white space runs found.");
		return true;
	}

	bool updatedSegmentation = true;
	while (updatedSegmentation) {

		//update status of remaining BCR
		updateBCRStatus();

		//remove groups that represent non-bcr runs or are too short
		updatedSegmentation = refineWhiteSpaceRuns();
	}

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
							sTls << tls;
						}
						else {
							QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx, (i - lastWSidx));
							tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
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
			sTls << tls;
		}
		else {
			QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx);
			tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
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

	QVector<QSharedPointer<WhiteSpacePixel>> isolatedBCR;

	for (auto bcr : mBcrM) {
		int wsCount = 0;

		for (int idx : pg.edgeIndexes(bcr->id())) {
			if (mBcrM.contains(qSharedPointerCast<WhiteSpacePixel>(pg.edges()[idx]->second())))
				++wsCount;
		}

		if (wsCount == 0)
			isolatedBCR << bcr;
	}

	//qInfo() << "there are " << mBcrM.size() << " white spaces";

	//remove isolated bcr (and merge neighboring text lines)
	deleteBCR(isolatedBCR);

	//qInfo() << isolatedBCR.size() << " isolated white spaces have been removed";
	//qInfo() << "there are " << mBcrM.size() << " white spaces remaining";
}

void WhiteSpaceSegmentation::deleteBCR(const QVector<QSharedPointer<WhiteSpacePixel>>& bcrM) {
	for (auto p : bcrM) {
		deleteBCR(p);
	}
}

void WhiteSpaceSegmentation::deleteBCR(const QSharedPointer<WhiteSpacePixel>& bcr) {

	//merge text lines neighboring the removable bcr
	auto ntl = mBcrNeighbors.value(bcr->id());

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
					auto ws_tmp1 = mBcrM.at(nextBCR_idx);

					while (true) {
						auto ws_tmp2 = mBcrM.at(nextBCR_idx);
						wsr->add(ws_tmp2);
						int downCount2 = udCount.value(ws_tmp2->id())[1];

						//trace white space run further
						if (downCount2 == 1) {

							for (int idx2 : edgeIndices.value(ws_tmp2->id())) {
								Rect fR2 = bcrEdges[idx2]->first()->bbox();
								Rect sR2 = bcrEdges[idx2]->second()->bbox();
								if (fR2.bottom() < sR2.top()) {
									auto ws_tmp3 = qSharedPointerCast<WhiteSpacePixel>(bcrEdges[idx2]->second());
									nextBCR_idx = mBcrM.indexOf(ws_tmp3);
								}
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

	for (auto bcr : mBcrM) {
		double bcrGap = bcr->bbox().width();
		for (auto n : mBcrNeighbors.value(bcr->id())) {
			if (bcrGap <= n->maxGap()) {
				bcr->setBCR(false);
				break;
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
		QString maxGap = QString::number(bcr->bbox().width());
		painter.drawText(bcr->bbox().toQRect(), Qt::AlignCenter, maxGap);
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

	//TODO improve polygon region output
	//TODO split text blocks into paragraphs

	if (mTextLines.isEmpty())
		return false;

	//sort text lines according to y_max coordinate for more efficient computation
	std::sort(mTextLines.begin(), mTextLines.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->boundingBox().top() < rhs->boundingBox().top();
	});

	computeAdjacency();
	formTextBlocks();

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
	
	//TODO change bounding polygon computation from qt polygon computations to openCV (qt leads to unexpected results)
	
	Rect bbox = Rect();
	QPolygonF poly;
	PixelSet pSet;
	
	//debug
	cv::Mat  result3;
	
	for(int i = 0; i < lines.length(); i++){

		auto l1 = lines[i];
		pSet.append(l1->pixels());

		if (i == 0) {
			poly = l1->convexHull().polygon();
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
			
			QPolygonF cut_poly1, cut_poly2;
			cut_poly1 = cut_poly.united(poly1);
			cut_poly2 = cut_poly.united(poly2);
			QPolygonF cut_poly_final = cut_poly1.united(cut_poly2);

			//final merged polygon
			poly = poly.united(cut_poly_final);

			//////////debug images

			//QImage qImg(Image::mat2QImage(mImg));
			//QPainter painter(&qImg);

			//painter.setPen(ColorManager::lightGray());
			//painter.drawPolygon(poly1);
			//painter.drawPolygon(poly2);

			//painter.setPen(ColorManager::blue());
			//painter.drawPolygon(hull_poly);
			//
			//painter.setPen(ColorManager::darkGray());
			//painter.drawPolygon(rect_poly);

			//painter.setPen(ColorManager::red());
			//painter.drawPolygon(cut_poly);

			//cv::Mat  result = Image::qImage2Mat(qImg);
			//painter.end();

			////////////

			//QImage qImg4(Image::mat2QImage(mImg));
			//painter.begin(&qImg4);

			//painter.setPen(ColorManager::red());
			//painter.drawPolygon(cut_poly);

			//cv::Mat  result4 = Image::qImage2Mat(qImg4);
			//painter.end();

			//////////////////

			//QImage qImg1(Image::mat2QImage(mImg));
			//painter.begin(&qImg1);

			//painter.setPen(ColorManager::blue());
			//painter.drawPolygon(cut_poly1);

			//cv::Mat  result1 = Image::qImage2Mat(qImg1);
			//painter.end();

			/////////////////

			//QImage qImg2(Image::mat2QImage(mImg));
			//painter.begin(&qImg2);

			//painter.setPen(ColorManager::blue());
			//painter.drawPolygon(cut_poly2);

			//cv::Mat  result2 = Image::qImage2Mat(qImg2);
			//painter.end();

			//////////////////

			//QImage qImg3(Image::mat2QImage(mImg));
			//painter.begin(&qImg3);

			//painter.setPen(ColorManager::blue());
			//painter.drawPolygon(poly);

			//result3 = Image::qImage2Mat(qImg3);
			//painter.end();
		}
		
		//if (bbox.isNull()) {
		//	bbox = l->boundingBox();
		//	poly = l->convexHull();
		//}

		//else {
		//	Polygon()
		//	poly = l->convexHull() poly.toPoints();
		//	bbox = bbox.joined(l->boundingBox());
		//}
	}

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
	//TODO check text width estimate computation

	//parameters
	//double maxHeightRatio = 2.5;
	double maxHeightRatio = 3.0;
	double minVertOverlapRatio = 0.33;
	//double distMultiplier = 5;
	double distMultiplier = 5;
	double textWidthEstimate = 0;	// individual value computed below
	bool filterEdges = true;		// based on maxHeightRatio + minVertOverlapRatio
	int maxRnnCount = 1;

	QVector<QSharedPointer<PixelEdge> > edges;
	QVector<QSharedPointer<Pixel>> mPixels(pixels);

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(mPixels.begin(), mPixels.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
	});

	//compute average line spacing -> *0.5 = text width estimate
	//QList<double> spacings;
	//for (const QSharedPointer<Pixel>& px : pixels) {

	//	if (!px->stats()) {
	//		qWarning() << "stats is NULL where it should not be...";
	//		continue;
	//	}
	//	spacings << px->stats()->lineSpacing();
	//}
	//textWidthEstimate = (Algorithms::statMoment(spacings, 0.5))*0.5;

	QList<double> spacings;
	for (auto p : pixels) {
		spacings << p->bbox().height();
	}

	textWidthEstimate = Algorithms::statMoment(spacings, 0.5);


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
				//textWidthEstimate = std::max(p1R.width(), p2R.width());
				//textWidthEstimate = (p1->stats()->lineSpacing())*0.5;
				
				if(xDist < textWidthEstimate*distMultiplier){
					rnn << p2;
				}
			}
		}
		
		//sort neighboring pixels according to their distance
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
						edges << QSharedPointer<PixelEdge>::create(p1, rnn.at(idx));	//add edges for remaining pairs
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

NNConnector::NNConnector(){
}

QVector<QSharedPointer<PixelEdge>> NNConnector::connect(const QVector<QSharedPointer<Pixel>>& pixels) const{

	//paramters
	double spacingMultiplier = 2.5;
	double lineSpacing = 0;			//computed below - average height of all components

	QVector<QSharedPointer<PixelEdge> > edges;
	QVector<QSharedPointer<Pixel>> mPixels(pixels);

	//sort pixels according to y_max coordinate for more efficient computation
	std::sort(mPixels.begin(), mPixels.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().top() < rhs->bbox().top();
	});

	//compute average height
	QList<double> spacings;
	for (const QSharedPointer<Pixel>& px : pixels) {
		spacings << px->bbox().height();
	}

	lineSpacing = Algorithms::statMoment(spacings, 0.5);

	for (const QSharedPointer<Pixel>& p1 : mPixels) {

		//hbbox
		double lb = std::max(p1->bbox().left() - (lineSpacing*spacingMultiplier), 0.0);
		double wb = p1->bbox().width() + ((lineSpacing*spacingMultiplier) * 2);
		Rect hbbox = Rect(lb, p1->bbox().top(), wb, p1->bbox().height());

		//vbbox
		double tb = std::max(p1->bbox().top() - (lineSpacing*spacingMultiplier), 0.0);
		double hb = p1->bbox().height() + ((lineSpacing*spacingMultiplier) * 2);
		Rect vbbox = Rect(p1->bbox().left(), tb, p1->bbox().width(), hb);

		for (const QSharedPointer<Pixel>& p2 : mPixels) {

			if (p2->bbox().top() > (tb + hb))
				break;

			if (p1->id() == p2->id())
				continue;
			
			if (p2->bbox().bottom() < tb)
				continue;

			if (p2->bbox().intersects(hbbox) || p2->bbox().intersects(vbbox)) {
				edges << QSharedPointer<PixelEdge>::create(p1, p2);
			}
		}
	}

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

	qInfo() << "Found #" << edges.size() << " white space edges";
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