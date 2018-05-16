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
	ScaleFactory::instance().init(img.size());

	mConfig = QSharedPointer<WhiteSpaceAnalysisConfig>::create();
	mConfig->loadSettings();
}

bool WhiteSpaceAnalysis::isEmpty() const {
	return mImg.empty();
}

bool WhiteSpaceAnalysis::compute() {
	//TODO improve initial set of components used for text line formation 
	//TODO use asssert() function to check input parameters and results
	
	qDebug() << "computing white spaces layout analysis...";

	mMinPixelsPerBlock = 3;

	if (!checkInput())
		return false;

	cv::Mat img = mImg;
	Timer dt;

	qDebug() << "scale factor dpi: " << ScaleFactory::scaleFactorDpi();
	img = ScaleFactory::scaled(img);
	mImg = img;

	SuperPixel sp = computeSuperPixels(img);

	//graph based superpixel text line clustering------------------------------------------------------

	//get pixel set
	PixelSet pSet = sp.pixelSet();

	if (pSet.isEmpty()) {
		qInfo() << "No super pixels found. Finished white space analysis";
		return false;
	}

	//compute stats (needed for line spacing used in clustering/distance computation)
	if (!computeLocalStats(pSet)){
		qWarning() << "Could not compute local stats for super pixels!";
		return false;
	}

	Rect lsR = filterPixels(pSet);
	
	TextLineHypothisizer tlh(mImg, pSet);
	// TODO add separator computation and use them in segementation process
	//tlh.addSeparatorLines(mStopLines);

	// compute initial text lines
	if (!tlh.compute()) {
		qWarning() << "Could not compute text line hypotheses!";
		return false;
	}

	auto  textLines = tlh.textLineSets();
	qInfo() << "Number of text lines is " << tlh.textLineSets().size();

	mTextLineHypotheses = textLines;
	//cv::Mat imgDebugWhiteSpaces = drawWhiteSpaces(img);	//based on mTextLineHypotheses and computed white spaces

	//----------------------------------------------------------- continue
	//computeWhiteSpaceSegmentation(textLines);

	WhiteSpaceSegmentation wss(mImg, textLines);
	
	// compute white space segmentation for estimated text lines
	if (!wss.compute()) {
		qWarning() << "Could not compute white space segmentation!";
		return false;
	}

	//get segmented text lines
	mWSTextLines = wss.textLineSets();
	//cv::Mat imgDebugWSS = wss.drawSplitTextLines(img);

	//obsolete should be done inm textblock formation
	//convertTextLines();	//convert tlc to textRegion objects

	TextBlockFormation tbf(mImg, mWSTextLines);
	
	if (!tbf.compute()) {
		qWarning() << "White space analysis: Could not compute text blocks!";
		return false;
	}
	
	mTextBlockSet = tbf.textBlockSet();
	
	mInfo << "white space layout analysis computed in" << dt;

	//draw pixel set-------------------------------------------------
	QImage qImg = Image::mat2QImage(img, true);
	QPainter painter(&qImg);

	painter.setPen(ColorManager::red());
	lsR.draw(painter);	//pixel filtering rect

	painter.setPen(ColorManager::blue());
	//pSet.draw(p_ps, PixelSet::draw_pixels);
	for (auto p : pSet.pixels()) {
		p->bbox().draw(painter);
	}
	painter.end();

	QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_pixelSet");
	cv::Mat img_debug = Image::qImage2Mat(qImg);
	Image::save(img_debug, imgPath);
	//---------------------------------------------------------------
	
	//draw text line hypotheses-------------------------------------------------
	cv::Mat img_tl = tlh.draw(img);
	imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_lineHypotheses");
	Image::save(img_tl, imgPath);
	//---------------------------------------------------------------

	//draw edges of Pixel graph-------------------------------------------------
	cv::Mat img_tl_ge = tlh.drawGraphEdges(img, ColorManager::green());
	imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_lineHypotheses_GraphEdges");
	Image::save(img_tl_ge, imgPath);
	//---------------------------------------------------------------

	// scale back to original coordinates
	ScaleFactory::scaleInv(mTextBlockSet);
	mTextBlockRegions = mTextBlockSet.toTextRegion();

	return true;

	////-------------------------------------------------------------------------------------------------

	////convert super pixels (sp) to text components (tc)
	//mBlobs = sp.getMserBlobs();
	//
	////draw blobs-------------------------------------------------
	//QImage qImg_blobs = Image::mat2QImage(img, true);
	//QPainter p_blobs(&qImg_blobs);
	//p_blobs.setPen(ColorManager::randColor());

	//for (auto b : mBlobs) {
	//	b->draw(p_blobs);
	//}

	//cv::Mat img_blobs = Image::qImage2Mat(qImg_blobs);
	//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_blobs");
	//Image::save(img_blobs, imgPath);
	////--------------------------------------------------------------

	//extractTextComponents();

	//computeInitialTLC();	//create initial text line candidates

	////draw tlc--------------------------------------------------------------
	//cv::Mat img_tlc = drawTLC(img);
	//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_initial_tlc");
	//Image::save(img_tlc, imgPath);
	////--------------------------------------------------------------

	////compute white space for each tlc
	//for (auto tlc : mTlcM) {
	//	tlc->computeWhiteSpaces(mImg.cols);
	//}

	////draw ws---------------------------------------------------
	//cv::Mat img_ws = drawWS(img);
	//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_initial_ws");
	//Image::save(img_ws, imgPath);
	////--------------------------------------------------------------

	//findBCR();		//mark between-column-rectangles (bcr), not surrounded by text components
	//splitTLC();		//split the tlc according to the bcr and create a list of the remaining ws
	//groupWS();		//form runs of white spaces

	////draw WSR--------------------------------------------------------------
	//cv::Mat img_wsr = drawWS(img);
	//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_final_ws");
	//Image::save(img_wsr, imgPath);
	////--------------------------------------------------------------


	//updateBCR();	//check if bcr are wider than the ws of their neighboring tlc
	//while (updateSegmentation()) {
	//	updateBCR();
	//}

	////draw tlc---------------------------------------------------
	//img_tlc = drawTLC(img);
	//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_final_tlc");
	//Image::save(img_tlc, imgPath);
	//qInfo() << "number of tlc components = " << mTlcM.length();
	////--------------------------------------------------------------

	////TODO check if sorting is necessary
	////sort text lines according to y coordinate
	//std::sort(mTlcM.begin(), mTlcM.end(), [](const auto& lhs, const auto& rhs) {
	//	return lhs->bbox().bottom() < rhs->bbox().bottom();
	//});

	//refineTLC();
	//
	////draw tlc--------------------------------------------------------------
	//img_tlc = drawTLC(img);
	//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_final_refined_tlc");
	//Image::save(img_tlc, imgPath);
	//qInfo() << "number of tlc components = " << mTlcM.length();
	////--------------------------------------------------------------

	//convertTLC();	//convert tlc to textRegion objects

	//TextBlockFormation tbf(mTextLines);
	//if (!tbf.compute()) {
	//	qWarning() << "White space analysis: Could not compute text blocks!";
	//	return false;
	//}

	//mTextBlocks = tbf.textBlocks();

	////for (int i = 0; i < mTlcM.length(); ++i){
	////	qInfo() << i << ": bbox = " << mTlcM.at(i)->bbox().toString() << " | annCount= " << annCount[i] << " | bnnIndices= " << bnnIndices[i];
	////}

	//// draw debug image-------------------------------------------------------------------
	////QImage qImg1 = Image::mat2QImage(img, true);
	////QImage qImg(qImg1.size(), qImg1.format());

	//QImage qImg = Image::mat2QImage(img, true);
	//QPainter p(&qImg);

	//for (QSharedPointer<TextLineCandidate> tlc : mTlcM) {

	//	for (QSharedPointer<TextComponent> tc : tlc->textComponents()) {
	//		
	//		//p.setPen(ColorManager::lightGray(0.5));
	//		//tc->mserBlob()->draw(p);

	//		if (!tc->hasRnn()) {
	//			//p.setPen(ColorManager::red(1.0));
	//			//tc->mserBlob()->draw(p);
	//		} 
	//		
	//		if (!tc->hasLnn()) {
	//			//p.setPen(ColorManager::blue(1.0));
	//			//tc->mserBlob()->draw(p);
	//		}
	//		
	//		if (tc->isAFork()) {
	//			//p.setPen(ColorManager::darkGray(1.0));			
	//			//tc->mserBlob()->draw(p);

	//			//for (auto b : tc->lnn()) {
	//			//	p.setPen(ColorManager::randColor());
	//			//	//b->mserBlob()->draw(p);
	//			//	b->drawLnns(p);
	//			//}
	//		}

	//		//if (tc->hasRnn()) {
	//		//	p.setPen(ColorManager::red(1.0));
	//		//	QPoint point1 = tc->mserBlob()->center().toQPoint();
	//		//	QPoint point2 = tc->rnn()->mserBlob()->center().toQPoint();
	//		//	p.drawLine(point1, point2);
	//		//}
	//	}

	//	for (auto ws : tlc->whiteSpaces()) {

	//		if (ws->isBCR()) {
	//			//p.setPen(ColorManager::red(1.0));
	//			//p.drawRect(ws->bbox().toQRect());
	//		}
	//		else {
	//			//p.setPen(ColorManager::red(0.5));
	//			//p.drawRect(ws->bbox().toQRect());
	//		}
	//	}

	//	//p.setPen(ColorManager::blue(1.0));
	//	//p.setPen(ColorManager::lightGray(1.0));
	//	//p.drawRect(tlc->bbox().toQRect());
	//}

	//for (QSharedPointer<TextLineCandidate> tlc : mTlcM) {
	//	p.setPen(ColorManager::green(1.0));
	//	p.drawRect(tlc->bbox().toQRect());
	//	
	//	QString maxGap = QString::number(tlc->maxGap());
	//	p.drawText(tlc->bbox().toQRect(), Qt::AlignCenter, maxGap);

	//	//for (auto tc : tlc->textComponents()) {
	//	//	p.setPen(ColorManager::lightGray(1.0));
	//	//	p.drawRect(tc->mserBlob()->bbox().toQRect());
	//	//}

	//	//for (auto ws : tlc->whiteSpaces()) {
	//	//	p.setPen(ColorManager::white(1.0));
	//	//	p.drawRect(ws->bbox().toQRect());
	//	//}
	//}

	//for (auto wsr : mWsrM) {
	//	
	//	for (auto ws : wsr->whiteSpaces()) {
	//		if (ws->isBCR()) {
	//			p.setPen(ColorManager::blue(1.0));
	//			p.drawRect(ws->bbox().toQRect());
	//		}
	//		else {
	//			p.setPen(ColorManager::red(1.0));
	//			p.drawRect(ws->bbox().toQRect());
	//		}

	//		QString gap = QString::number(ws->bbox().width());
	//		p.drawText(ws->bbox().bottomLeft().toQPoint(), gap);	
	//	}
	//}

	//mRnnImg = Image::qImage2Mat(qImg);

	////mRnnImg = sp.draw(img);
	////if (img.rows > 2000) {
	////	// if you stumble upon this line:
	////	// microfilm images are binary with heavy noise
	////	// a small median filter fixes this issue...
	////	cv::medianBlur(img, img, 3);
	////}

	//mInfo << "computed in" << dt;

	//return true;
}

QSharedPointer<WhiteSpaceAnalysisConfig> WhiteSpaceAnalysis::config() const {
	return qSharedPointerDynamicCast<WhiteSpaceAnalysisConfig>(mConfig);
}

SuperPixel WhiteSpaceAnalysis::computeSuperPixels(const cv::Mat & img){

	//TODO FIX PARAMETERS
	int NumErosionLayers = 1; //must be > 0

	// compute super pixels
	SuperPixel sp = SuperPixel(img);
	
	//changing sp parameters here
	auto spConfig = sp.config();
	spConfig->setNumErosionLayers(NumErosionLayers);
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

Rect WhiteSpaceAnalysis::filterPixels(PixelSet& pSet) const{

	if (pSet.isEmpty())
		return Rect();

	QVector<QString> removeIDs;

	//filter pixels according to size constraints
	double ls = pSet.lineSpacing(0.5);
	double hLimit = 1.5 * ls;
	double wLimit = 1 * ls;

	Rect lsR(0, 0, wLimit, hLimit);
	for (auto p : pSet.pixels()) {
		if (p->bbox().height() > hLimit || p->bbox().width() > wLimit) {
			removeIDs << p->id();
		}
	}


	//filter overlapping pixels
	for (auto p1 : pSet.pixels()) {

		if (removeIDs.contains(p1->id()))
			continue;

		for (auto p2 : pSet.pixels()) {

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
		pSet.remove(pSet.find(id));
	}

	return lsR;
}

//void WhiteSpaceAnalysis::computeWhiteSpaceSegmentation(QVector<QSharedPointer<WSTextLineSet>>& textLines){
//
//	//split text lines according to white spaces (bcr)	
//	for (auto tl : textLines) {
//
//		QVector<QSharedPointer<Pixel>> textPixels(tl->pixels());
//		std::sort(textPixels.begin(), textPixels.end(), [](const auto& lhs, const auto& rhs) {
//			return lhs->bbox().right() < rhs->bbox().right();
//		});
//
//		QVector<QSharedPointer<WhiteSpacePixel>> wsPixels(tl->whiteSpacePixels());
//		std::sort(wsPixels.begin(), wsPixels.end(), [](const auto& lhs, const auto& rhs) {
//			return lhs->bbox().right() < rhs->bbox().right();
//		});
//
//		int lastTPidx = 0;
//		int lastWSidx = 0;
//
//		if (wsPixels.isEmpty()) {
//			splitTextLines << tl;
//			continue;
//		}
//
//		for (int i = 0; i < wsPixels.size(); ++i) {
//
//			auto ws = wsPixels[i];
//
//			//split text line at bcr
//			if (ws->isBCR()) {
//				bcrSet << ws;
//
//				for (int j = lastTPidx; textPixels.size(); ++j) {
//					if (textPixels[j]->bbox().right() > ws->bbox().left()) {
//
//						QSharedPointer<WSTextLineSet> tls;
//						QVector<QSharedPointer<Pixel>> stp = textPixels.mid(lastTPidx, (j - lastTPidx));
//
//						if (i == 0 || i == (wsPixels.size() - 1) || (i - lastWSidx) == 0) {
//							tls = QSharedPointer<WSTextLineSet>::create(stp);
//							splitTextLines << tls;
//						}
//						else {
//							QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx, (i - lastWSidx));
//							tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
//							splitTextLines << tls;
//						}
//
//						//save indices of text line sets neighboring bcr
//						if (lastWSidx != 0) {
//							QString key = bcrSet[bcrSet.size() - 2]->id();
//							QVector<QSharedPointer<WSTextLineSet>> value = mBcrNeighbors.value(key);
//							value << tls;
//							mBcrNeighbors.insert(key, value);
//						}
//
//						QVector<QSharedPointer<WSTextLineSet>> value(1, tls);
//						mBcrNeighbors.insert(ws->id(), value);
//
//						lastTPidx = j;
//						lastWSidx = i+1;
//						break;
//					}
//				}
//			}
//		}
//		
//		//add text line containing pixels after last bcr
//		QSharedPointer<WSTextLineSet> tls;
//		QVector<QSharedPointer<Pixel>> stp = textPixels.mid(lastTPidx);
//		
//		if (lastWSidx > (wsPixels.size() - 1)) {	//bcr is last white space in text line
//			tls = QSharedPointer<WSTextLineSet>::create(stp);
//			splitTextLines << tls;
//		}
//		else {
//			QVector<QSharedPointer<WhiteSpacePixel>> sws = wsPixels.mid(lastWSidx);
//			tls = QSharedPointer<WSTextLineSet>::create(stp, sws);
//			splitTextLines << tls;
//		}
//
//		QString key = bcrSet[bcrSet.size() - 1]->id();
//		QVector<QSharedPointer<WSTextLineSet>> value = mBcrNeighbors.value(key);
//		value << tls;
//		mBcrNeighbors.insert(key, value);
//	}
//
//	//draw split text lines and white spaces-------------------------------------------------
//	QImage qImg = Image::mat2QImage(mImg, true);
//	QPainter painter(&qImg);
//
//	painter.setPen(ColorManager::blue());
//	for (auto tl : splitTextLines) {
//		tl->boundingBox().draw(painter);
//	}
//
//	painter.setPen(ColorManager::red());
//	for (auto bcr : bcrSet) {
//		bcr->bbox().draw(painter);
//	}
//
//	painter.end();
//
//	QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_split_components");
//	cv::Mat img_debug = Image::qImage2Mat(qImg);
//	Image::save(img_debug, imgPath);
//	//---------------------------------------------------------------
//
//	// compute pixel graph for segmentation regions
//	if (splitTextLines.isEmpty() || bcrSet.isEmpty()) {
//		qWarning() << "Could not compute white space segmentation.";
//		qWarning() << "No text lines or white spaces found!";
//		return;
//	}
//	
//	PixelSet wsSet;
//	QVector<QSharedPointer<TextRegionPixel>> trSet;
//
//	//create pixel set for white space computation (text regions + BCR)
//	for (auto sl : splitTextLines) {
//		QSharedPointer<TextRegionPixel> trPixel = sl->convertToPixel();
//		trSet << trPixel;
//		wsSet.add(qSharedPointerCast<Pixel>(trPixel));
//	}
//
//	for (auto bcr : bcrSet) {
//		wsSet.add(qSharedPointerCast<Pixel>(bcr));
//		//wsSet.add(bcr);
//	}
//
//	//compute LineSpacing
//	QList<double> spacings;
//	for (auto tl : textLines) {
//		spacings << tl->lineSpacing();
//	}
//	double lineSpacing = Algorithms::statMoment(spacings, 0.5);
//	qInfo() << "Line spacing = " << lineSpacing;
//
//	//find horizontally overalapping white spaces
//	WSConnector wspc;
//	wspc.setLineSpacing(lineSpacing);
//
//	PixelGraph pg(wsSet);
//	//pg.connect(nnpc, PixelGraph::sort_edges);
//
//	pg.connect(wspc);
//
//	//TODO remove bcr with no connections to other bcr 
//	// or remove bcr that only have connections to text pixels?
//	removeIsolatedBCR(pg);
//	wsrM = findWhiteSpaceRuns(pg);
//
//	bool updated = true;
//	while (updated) {
//		//update status of remaining BCR
//		updateBCRStatus();
//
//		//remove groups that represent non-bcr runs or are too short
//		updated = refineWhiteSpaceRuns();
//	}
//
//	//TODO further refinment of text lines!?
//
//	//updateBCR();	//check if bcr are wider than the ws of their neighboring tlc
//	//while (updateSegmentation()) {
//	//	updateBCR();
//	//}
//
//	//refineTLC();
//
//	//convertTLC();	//convert tlc to textRegion objects
//	//TextBlockFormation tbf(mTextLines);
//	//if (!tbf.compute()) {
//	//	qWarning() << "White space analysis: Could not compute text blocks!";
//	//}
//
//
//	//draw white space graph-------------------------------------------------
//	qImg = Image::mat2QImage(mImg, true);
//	painter.begin(&qImg);
//	painter.setPen(ColorManager::green());
//	//pg.draw(painter);
//
//	painter.setPen(ColorManager::blue());
//	for (auto stl : splitTextLines) {
//		stl->boundingBox().draw(painter);
//	}
//
//	for (auto bcr : bcrSet) {
//		//painter.setPen(ColorManager::darkGray());
//		//if(bcr->isBCR())
//			painter.setPen(ColorManager::red());
//		bcr->bbox().draw(painter);
//	}
//
//	painter.end();
//
//	cv::Mat img_pg_ws = Image::qImage2Mat(qImg);
//	imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_whiteSpaceSegmentation");
//	Image::save(img_pg_ws, imgPath);
//	//---------------------------------------------------------------
//}

//void WhiteSpaceAnalysis::convertTextLines() {
//	//convert text lines to xml text regions
//	for (auto tl : mWSTextLines) {
//		QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());
//		textRegion->setPolygon(rdf::Polygon::fromRect(tl->boundingBox()));
//		textRegion->setId(textRegion->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors
//		textRegion->setType(rdf::Region::type_text_line);
//
//		mTextLineRegions.append(textRegion);
//	}
//}

cv::Mat WhiteSpaceAnalysis::drawWhiteSpaces(const cv::Mat & img, const QColor & col){
	
	QColor inputColor = col;
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



void WhiteSpaceAnalysis::extractTextComponents(){

	for (QSharedPointer<MserBlob> b : mBlobs) {
		QSharedPointer<TextComponent> tc = QSharedPointer<TextComponent>(new TextComponent(b));
		mTcM.append(tc);
	}

	//TODO use additional filtering to reduce amount of noise components
	//save a list of final tc used for further processing

	//sort tc according to y_max coordinate for more efficient computation
	//auto tcM_y = QVector<QSharedPointer<TextComponent>>(tcM);

	std::sort(mTcM.begin(), mTcM.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->mserBlob()->bbox().top() < rhs->mserBlob()->bbox().top();
	});

	//find right nearest neighbors (rnn) for each text component
	for (QSharedPointer<TextComponent> tc : mTcM) {
		QSharedPointer<TextComponent> rnn;
		bool foundRnn = false;

		for (QSharedPointer<TextComponent> tcn : mTcM) {

			QSharedPointer<MserBlob> tcBlob = tc->mserBlob();
			QSharedPointer<MserBlob> tcnBlob = tcn->mserBlob();

			if (tcBlob != tcnBlob) {
				
				Rect tcR = tcBlob->bbox();
				if (tc->hasLnn())
					tcR = tcR.joined(tc->lnn().at(tc->bestFitLnnIdx())->mserBlob()->bbox());

				Rect tcnR = tcnBlob->bbox();

				//qDebug() << "tcnR is: " << tcnR.toString();
				Vector2D irSize(mImg.size().width - tcR.width(), tcR.height());
				Rect ir = Rect(tcR.topRight(), irSize);

				if (tcnR.center().x() > tcR.right() && ir.intersects(tcnR)) {	//center of neighbor should not be within components rect
					//TODO check if previous condition is needed or could be replaced

					//check height ratio
					double hRatio = tcR.height() / tcnR.height();
					if (0.4 < hRatio && hRatio < 2.5){
						//check if relative y-overlap is bigger than 1/3 of bigger component
						double yOverlap = std::min(tcR.bottom(), tcnR.bottom()) - std::max(tcR.top(), tcnR.top());
						double relYoverlap = yOverlap / std::max(tcR.height(), tcnR.height());
						if (yOverlap > 0 && relYoverlap > (0.33)) {
							
							if (foundRnn) {
								if (tcnR.left() < rnn->mserBlob()->bbox().left()) {
									rnn = tcn;
								}
							}
							else {
								rnn = tcn;
								foundRnn = true;
							}
						}
					}
				}
				else if (tcnR.top()>tcR.bottom()) {
					break;
				}
			}
		}

		// if found  set rnn for current tc
		if (!rnn.isNull()) {
			tc->setRnn(rnn);
			rnn->setLnn(tc);

			if (rnn->lnn().size() > 1) {
				rnn->setForkMarker(true);
			}
		}
	}
}

void WhiteSpaceAnalysis::computeInitialTLC(){

	//create initial text line candidates
	for (auto tc : mTcM) {
		if (!tc->hasLnn()) {
			//create text line candidates
			QSharedPointer<TextLineCandidate> tlc;
			tlc = QSharedPointer<TextLineCandidate>(new TextLineCandidate(tc));
			mTlcM.append(tlc);
		}
	}

	//sort text lines according to y coordinate
	std::sort(mTlcM.begin(), mTlcM.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().bottom() < rhs->bbox().bottom();
	});

	refineTLC();
}

void WhiteSpaceAnalysis::findBCR(){

	//remove isolated white space rectangles
	//TODO find more efficient way of comparing locations of white spaces
	for (int i = 0; i < mTlcM.length(); ++i) {
		for (auto ws1 : mTlcM.at(i)->whiteSpaces()) {
			if (ws1->isBCR()) {
				bool topIsIsolated = true;
				bool bottomIsIsolated = true;
				Rect ws1B = ws1->bbox();

				//look for white spaces below the current one
				if (i + 1 < mTlcM.length()) {
					for (int j = i + 1; j < mTlcM.length(); ++j) {
						Rect tlcB = mTlcM.at(j)->bbox();
						//check if x span of text line covers current white space
						if (tlcB.left() < ws1B.left() && tlcB.right() > ws1B.right()) {

							for (auto ws2 : mTlcM.at(j)->whiteSpaces()) {
								if (ws2->isBCR()) {
									Rect ws2B = ws2->bbox();
									Rect ir(ws1B.left(), ws2B.top(), ws1B.width(), 1);
									if (ws2B.intersects(ir)) {
										ws2->setHasANN(true);
										ws1->addBnn(ws2);
										bottomIsIsolated = false;
										//break; //look at white spaces below in order to determine all bnn
									}
								}
							}
							break;	//only look at first text line below current ws
						}
					}
				}

				//look for white spaces above the current one
				if (i - 1 >= 0) {
					for (int j = i - 1; j >= 0; --j) {
						Rect tlcB = mTlcM.at(j)->bbox();
						//check if x span of text line covers current white space
						if (tlcB.left() < ws1B.left() && tlcB.right() > ws1B.right()) {

							for (auto ws2 : mTlcM.at(j)->whiteSpaces()) {
								if (ws2->isBCR()) {
									Rect ws2B = ws2->bbox();
									Rect ir(ws1B.left(), ws2B.top(), ws1B.width(), 1);
									if (ws2B.intersects(ir)) {
										topIsIsolated = false;
										break;
									}
								}
							}
							break;	//only look at first text line above current ws
						}
					}
				}

				if (topIsIsolated && bottomIsIsolated) {
					ws1->setIsBCR(false);
				}
			}
		}
	}
}

void WhiteSpaceAnalysis::splitTLC(){

	//TODO refactor this process for efficiency
	//TODO order of mTlcM should be kept 
	//TODO check validity of split tlc (location, size, etc.)
	QVector<QSharedPointer<TextLineCandidate>> tlcM_tmp;

	//split text lines according to bcr and compute maxGaps
	for (auto tlc : mTlcM) {

		//add text lines without white spaces
		if (tlc->whiteSpaces().length() == 0) {
			tlcM_tmp.append(tlc);
			continue;
		}

		auto tlc_ws = tlc->whiteSpaces();
		auto tlc_tc = tlc->textComponents();
		double maxGap = 0;
		int previousWS_idx = -1;
		int lastTC_idx = -1;

		for (int i = 0; i < tlc_ws.length(); ++i) {
			auto ws = tlc_ws.at(i);

			if (ws->isBCR()) {
				mWsM.append(ws);

				int firstTC_idx = lastTC_idx + 1;

				//find tc located left from current ws
				for (int j = firstTC_idx; j < tlc_tc.length(); ++j) {
					double rB = tlc_tc.at(j)->mserBlob()->bbox().right();

					if (rB == ws->bbox().left()) {
						lastTC_idx = j;
						break;
					}
				}

				//create new tlc and set parameter
				QSharedPointer<TextLineCandidate> tlc_tmp(new TextLineCandidate());
				int tc_rl = lastTC_idx - firstTC_idx + 1;
				tlc_tmp->setTextComponents(tlc_tc.mid(firstTC_idx, tc_rl));

				if (previousWS_idx + 1 <= i - 1) {
					int ws_rl = i - (previousWS_idx + 1);
					tlc_tmp->setWhiteSpaces(tlc_ws.mid(previousWS_idx + 1, ws_rl));
				}
				else {
					////qWarning() << "Trying to split tlc but resulting tlc has no ws!";
					tlc_tmp->setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>>());
					//TODO create white space vector object in tlc constructor and skip this step
				}

				tlc_tmp->setMaxGap(maxGap);
				tlcM_tmp.append(tlc_tmp);
				
				maxGap = 0;
				previousWS_idx = i;
			}
			else {
				//update maxGap
				double gap = ws->bbox().width();
				if (gap > maxGap)
					maxGap = gap;
			}

			//add last text line
			if (i == tlc_ws.length() - 1) {

				QSharedPointer<TextLineCandidate> tlc_tmp(new TextLineCandidate());
				int tc_rl = tlc_tc.length() - (lastTC_idx + 1);
				tlc_tmp->setTextComponents(tlc_tc.mid(lastTC_idx + 1, tc_rl));

				if (previousWS_idx + 1 <= i) {
					int ws_rl = tlc_ws.length() - (previousWS_idx + 1);
					tlc_tmp->setWhiteSpaces(tlc_ws.mid(previousWS_idx + 1, ws_rl));
				}
				else {
					//qWarning() << "Trying to split tlc but resulting tlc has no ws!";
					tlc_tmp->setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>>());
					//TODO create white space vector object in tlc constructor and skip this step
				}

				tlc_tmp->setMaxGap(maxGap);
				tlcM_tmp.append(tlc_tmp);
			}
		}
	}

	mTlcM = tlcM_tmp;
}

void WhiteSpaceAnalysis::groupWS() {

	//find white space runs (wsr)
	for (auto ws : mWsM) {

		if (!ws->hasANN() || ws->bnn().length() > 1) {
			for (auto bws : ws->bnn()) {
				auto wsr = QSharedPointer<WSRun>(new WSRun(ws));
				wsr->appendAllBnn(bws);
				mWsrM.append(wsr);
			}
		}
	}
}

void WhiteSpaceAnalysis::updateBCR() {

	//refine list of wsr runs
	for (auto ws : mWsM) {
		Rect wsB = ws->bbox();
		double mg = 0;

		for (auto tlc : mTlcM) {
			Rect tlcB = tlc->bbox();

			//check if y span of text line covers current white space
			if (wsB.top() >= tlcB.top() && wsB.bottom() <= tlcB.bottom()) {

				//TODO avoid iterating over all tlcs
				//TODO check if both tlc on left and right side have only one word -> setIsBCR(false)
				if (wsB.left() == tlcB.right() || wsB.right() == tlcB.left()) {
					if (tlc->maxGap() > mg)
						mg = tlc->maxGap();

					if (mg >= wsB.width()) {
						ws->setIsBCR(false);
						break;
					}
				}
			}
		}
	}
}

bool WhiteSpaceAnalysis::updateSegmentation() {

	bool updated = false;
	auto wsrM = mWsrM;
	
	for (auto wsr : wsrM) {

		int bcr_count = 0;
		auto wsM = wsr->whiteSpaces();
		for (auto ws : wsM) {
			if (ws->isBCR())
				++bcr_count;
		}

		//delete wsr with only one element or containing no bcr
		if (bcr_count == 0 || wsM.length()==1) {
			
			if (mWsrM.indexOf(wsr) == -1)
				qWarning() << "Couldn't find wsr that should be deleted" ;

			if (deleteWSR(wsr)) {
				updated = true;
				continue;
			}		
		}

		if (trimWSR(wsr))
			updated = true;
	}

	return updated;
}

bool WhiteSpaceAnalysis::deleteWS(QSharedPointer<WhiteSpace> ws) {
	
	//find tlc neighboring ws and merge them
	int tlcIdx1 = -1;
	int tlcIdx2 = -1;
	for (int j = 0; j < mTlcM.length(); j++) {
		Rect wsB = ws->bbox();
		Rect tlcB = mTlcM.at(j)->bbox();
		if (wsB.top() >= tlcB.top() && wsB.bottom() <= tlcB.bottom()) {
			//if (mTlcM.at(j)->bbox().intersects(ws->bbox())) {
			if (wsB.left() == tlcB.right()) {
				tlcIdx1 = j;
			}
			if (wsB.right() == tlcB.left()) {
				tlcIdx2 = j;
			}
		}
	}

	if (tlcIdx1 == -1 || tlcIdx2 == -1) {
		//TODO check why this happens
		qWarning() << "White space analysis: Failed to delete WS!";
		return false;
	}

	auto tlc1 = mTlcM.at(tlcIdx1);
	auto tlc2 = mTlcM.at(tlcIdx2);

	if (!tlc1->merge(tlc2, ws))
		return false;

	if (tlc1->bbox().bottom() > tlc1->bbox().bottom()) {
		mTlcM.replace(tlcIdx1, tlc1);
		mTlcM.remove(tlcIdx2);
	}
	else {
		mTlcM.replace(tlcIdx2, tlc1);
		mTlcM.remove(tlcIdx1);
	}

	mWsM.removeAt(mWsM.indexOf(ws));	//remove ws

	return true;
}

bool WhiteSpaceAnalysis::deleteWSR(QSharedPointer<WSRun> wsr) {
	
	auto wsM = wsr->whiteSpaces();
	bool updated = false;

	for (int i = 0; i < wsM.length(); i++) {
		auto ws = wsM.at(i);

		if (mWsM.indexOf(wsM.at(i)) == -1) {
			continue; // nothin to do, ws already deleted
		}

		//remove ws in wsr with one or less bnn and merge tlc for each removed ws
		if (ws->bnn().length() <= 1) {
			if (ws->bnn().length() == 1 && (i+1) > wsM.length()) {
				continue;	//ws is still contained in another wsr
			}

			if (deleteWS(ws))
				updated = true;
		}

		//if there ís more than one bnn remove the following ws from the bnn list
		//the current ws should then be deleted if all except for one bnn have been removed
		if (ws->bnn().length() > 1) {
			if (wsM.length() > i + 1) {
				//TODO check if this works
				qInfo() << "bnn list length = " << ws->bnn().length() << " before removing";
				ws->bnn().removeAt(ws->bnn().indexOf(wsM.at(i + 1)));
				qInfo() << "bnn list length = " << ws->bnn().length() << " after removing";
			}
		}
	}

	//updated = true;
	mWsrM.removeAt(mWsrM.indexOf(wsr));	//remove wsr

	return updated;
}

bool WhiteSpaceAnalysis::trimWSR(QSharedPointer<WSRun> wsr){

	auto wsM = wsr->whiteSpaces();
	bool updated = false;

	//use this to shorten the wsr
	if (wsM.size() > 1 && !wsM.last()->isBCR()) {

		auto bnn = wsM.at(wsM.length() - 2)->bnn();
		bnn.removeAll(wsM.last());
		wsM.at(wsM.length() - 2)->setBnn(bnn);

		if (mWsM.indexOf(wsM.last()) != -1) {
			deleteWS(wsM.last()); //TODO nothin to do, ws already deleted -> check why this is happening
		}
		//deleteWS(wsM.last());

		wsM.removeAt(wsM.length() - 1);
		updated = true;
	}

	if (wsM.size() > 1 && !wsM.at(0)->isBCR()) {
		wsM.at(1)->setHasANN(false);
		
		if (mWsM.indexOf(wsM.at(0)) != -1) {
			deleteWS(wsM.at(0)); // nothin to do, ws already deleted
		}
		//deleteWS(wsM.at(0));

		wsM.removeAt(0);
		updated = true;
	}

	if (updated) {
		wsr->setWhiteSpaces(wsM);
	}	

	return updated;
}

void WhiteSpaceAnalysis::convertTLC() {
	//convert tlc to text regions
	for (auto tlc : mTlcM) {
		QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());
		textRegion->setPolygon(rdf::Polygon::fromRect(tlc->bbox()));
		textRegion->setId(textRegion->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors
		textRegion->setType(rdf::Region::type_text_line);

		mTextLineRegions.append(textRegion);
	}
}

cv::Mat WhiteSpaceAnalysis::drawTLC(cv::Mat img){
	
	QImage qImg_tlc = Image::mat2QImage(img, true);
	QImage qImg_blank(qImg_tlc.size(), qImg_tlc.format());

	//use blank or input image as background
	QImage qImg = qImg_tlc;
	QPainter p(&qImg);


	for (QSharedPointer<TextLineCandidate> tlc : mTlcM) {

		p.setPen(ColorManager::randColor());
		for (int i = 0; i < tlc->textComponents().length(); ++i) {
			auto tc = tlc->textComponents().at(i);

			//p.setPen(ColorManager::lightGray(0.5));
			tc->mserBlob()->draw(p);
			p.drawRect(tc->mserBlob()->bbox().toQRect());

			//if (!tc->hasRnn()) {
			//	p.setPen(ColorManager::darkGray(1.0));
			//	tc->mserBlob()->draw(p);
			//}

			//if (!tc->hasLnn()) {
			//	p.setPen(ColorManager::green(1.0));
			//	tc->mserBlob()->draw(p);
			//}

			//if (tc->isAFork()) {
			//	p.setPen(ColorManager::red(1.0));
			//	//p.setPen(ColorManager::darkGray(1.0));			
			//	tc->mserBlob()->draw(p);

				//for (auto b : tc->lnn()) {
				//	p.setPen(ColorManager::randColor());
				//	//b->mserBlob()->draw(p);
				//	b->drawLnns(p);
				//}
			//}

			if (i!=0) {
				//p.setPen(ColorManager::blue(1.0));
				auto tc2 = tlc->textComponents().at(i-1);
				QPoint point1 = tc->mserBlob()->center().toQPoint();
				QPoint point2 = tc2->mserBlob()->center().toQPoint();
				p.drawLine(point1, point2);
			}
		}

		p.setPen(ColorManager::blue(1.0));
		p.drawRect(tlc->bbox().toQRect());

		QString maxGap = QString::number(tlc->maxGap());
		p.setPen(ColorManager::darkGray(1.0));
		p.drawText(tlc->bbox().toQRect(), Qt::AlignCenter, maxGap);
	}

	cv::Mat img_out = Image::qImage2Mat(qImg);
	return img_out;
}

cv::Mat WhiteSpaceAnalysis::drawWS(cv::Mat img) {
	
	QImage qImg_ws = Image::mat2QImage(img, true);
	QImage qImg_blank(qImg_ws.size(), qImg_ws.format());
	
	//use blank or input image as background
	QImage qImg = qImg_blank;
	QPainter p(&qImg);

	//white space output color 
	p.setPen(ColorManager::blue(1.0));

	if (mWsrM.isEmpty()) {
		for (auto tlc : mTlcM) {
			for (auto ws : tlc->whiteSpaces()) {
				p.drawRect(ws->bbox().toQRect());

				//if (ws->isBCR()) {
				//	p.setPen(ColorManager::red(1.0));
				//	p.drawRect(ws->bbox().toQRect());
				//}
			}
		}

	}
	else {
		for (auto wsr : mWsrM) {			
			for (auto ws : wsr->whiteSpaces()) {
				p.drawRect(ws->bbox().toQRect());

				//if (ws->isBCR()) {
				//	p.setPen(ColorManager::red(1.0));
				//	p.drawRect(ws->bbox().toQRect());
				//}
			}
		}
	}

	cv::Mat img_out = Image::qImage2Mat(qImg);
	return img_out;
}

void WhiteSpaceAnalysis::refineTLC() {
	
	//remove nested/overlapping text line  candidates
	//merge text lines with large y overlap that are very close to each other
	QVector<QSharedPointer<TextLineCandidate>> tlcM_final;
	for (auto tlc1 : mTlcM) {
		bool final = true;

		for (auto tlc2 : mTlcM) {
			if (tlc1 != tlc2) {
				Rect b1 = tlc1->bbox();
				Rect b2 = tlc2->bbox();

				//merge nested or overlapping tlc
				if (b1.area() < b2.area()) {
					if (b1.intersects(b2)) {
						double overlapArea = (b1.intersected(b2)).area();
						//TODO perform a more elaborate check to
						//avoid merging slanted text lines where text components actually do not overlap
						if ((overlapArea / b1.area()) > double(0.75)) {
							final = false;
							break;
						}
						if ((overlapArea / b1.area()) > double(0.75)) {
							if (tlc1->textComponents().length() < 10) {
								final = false;
								break;
							}
						}
					}

					double xOverlap = std::min(b1.right(), b2.right()) - std::max(b1.left(), b2.left());
					double yOverlap = std::min(b1.bottom(), b2.bottom()) - std::max(b1.top(), b2.top());
					if (xOverlap > -5 && yOverlap > 0) {
						double relYoverlap = yOverlap / std::min(b1.height(), b2.height());
						double relHeight = b1.height() / b2.height();
						if (relYoverlap > 0.75 && relHeight < 1.5) {
							final = false;
							tlc2->merge(tlc1);
							//mTlcM.replace(mTlcM.indexOf(tlc2), merge_tlc);
							break;
						}
					}
				}
			}
		}

		if (tlc1->textComponents().length() < 3) {
			continue;
		}

		if (final) {
			tlcM_final.append(tlc1);
		}
	}

	mTlcM = tlcM_final;
}

QVector<QSharedPointer<TextRegion>> WhiteSpaceAnalysis::textLines(){
	
	QVector<QSharedPointer<Region> > textRegions = RegionManager::filter<Region>(mTextBlockRegions, Region::type_text_line);
	for (auto tr : textRegions) {
		mTextLineRegions << qSharedPointerCast<TextRegion>(tr);
	}

	return mTextLineRegions;
}

QSharedPointer<Region> WhiteSpaceAnalysis::textBlocks() {
	return mTextBlockRegions;
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

bool TextLineCandidate::merge(QSharedPointer<TextLineCandidate> tlc, QSharedPointer<WhiteSpace> ws){

	//TODO if needed to exchange tlc try handing over this and call merge again
	
	double ws_width = 0;
	if (!ws.isNull()) {
		ws_width = ws->bbox().width();
	}
	
	if(bbox().right() < tlc->bbox().left()){
		
		mTextComponents.append(tlc->textComponents());
		if (!ws.isNull())
			mWhiteSpaces.append(ws);
		mWhiteSpaces.append(tlc->whiteSpaces());
		mMaxGap = std::max(mMaxGap, tlc->maxGap());
		mMaxGap = std::max(mMaxGap, std::max(ws_width, tlc->maxGap()));
	}
	else {
		if (tlc->bbox().right() < bbox().left()) {
			auto tc_tmp = tlc->textComponents();
			tc_tmp.append(mTextComponents);
			mTextComponents = tc_tmp;

			auto ws_tmp = tlc->whiteSpaces();
			if (!ws.isNull())
				ws_tmp.append(ws);
			ws_tmp.append(mWhiteSpaces);
			mWhiteSpaces = ws_tmp;

			mMaxGap = std::max(mMaxGap, std::max(ws_width, tlc->maxGap()));
		}
		else {
			if (!ws.isNull()) {
				qWarning() << "White space analysis: Failed to merge tlc because they seem to be overlapping";
				qWarning() << "White space analysis: Need to recompute white spaces";
				qWarning() << "White space analysis: Recomputation of white spaces not possible";
				
				return false;
			}

			mTextComponents.append(tlc->textComponents());
			//sort text components according to their max x coordinate
			std::sort(mTextComponents.begin(), mTextComponents.end(), [](const auto& lhs, const auto& rhs) {
				return lhs->mserBlob()->bbox().right() < rhs->mserBlob()->bbox().right();
			});
		}
	}
	
	return true;
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

void TextLineCandidate::setTextComponents(QVector<QSharedPointer<TextComponent>> textComponents){
	mTextComponents = textComponents;
}

void TextLineCandidate::setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>> whiteSpaces){
	mWhiteSpaces = whiteSpaces;
}

void TextLineCandidate::setMaxGap(double maxGap){
	mMaxGap = maxGap;
}

double rdf::TextLineCandidate::maxGap() const{
	return mMaxGap;
}

QVector<QSharedPointer<TextComponent>> TextLineCandidate::textComponents() const{
	return mTextComponents;
}

QVector<QSharedPointer<WhiteSpace>> TextLineCandidate::whiteSpaces() const{
	return mWhiteSpaces;
}

void TextLineCandidate::computeWhiteSpaces(int pageWidth){

	if (!mWhiteSpaces.isEmpty()) {
		qWarning() << "White space analysis: Recomputing white spaces of tlc.";
		mWhiteSpaces.clear();
	}

	for (int i = 1; i < mTextComponents.length(); ++i) {
		Rect r1 = mTextComponents.at(i - 1)->mserBlob()->bbox();
		Rect r2 = mTextComponents.at(i)->mserBlob()->bbox();
		
		if (r2.left() - r1.right() > 0) {

			double wsrTop = std::max(r1.top(), r2.top());
			double wsrBottom = std::min(r1.bottom(), r2.bottom());
			Vector2D wsrTL = Vector2D(r1.right(), wsrTop);

			Vector2D wsrSize = Vector2D(r2.left() - r1.right(), wsrBottom - wsrTop);

			Rect wsr(wsrTL, wsrSize);
			QSharedPointer<WhiteSpace> ws(new WhiteSpace(wsr));
			mWhiteSpaces.append(ws);

		}
	}

	//sort white spaces according to size
	QVector<QSharedPointer<WhiteSpace>> ws_l(mWhiteSpaces);

	std::sort(ws_l.begin(), ws_l.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().width() > rhs->bbox().width();
	});

	//compute max number of between column rectangles and mark them
	//TODO compute this parameter in the text line creation
	double tlcWdith = bbox().size().length();
	double bcrn = std::max(1.0, std::round(8 * tlcWdith / pageWidth));

	if (ws_l.length() < bcrn)
		bcrn = ws_l.size();

	for (int i = 0; i < bcrn; ++i) {
		ws_l.at(i)->setIsBCR(true);
	}
}

void TextLineCandidate::appendAllRnn(const QSharedPointer<TextComponent> tc){

	QSharedPointer<TextComponent> next = tc->rnn();

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
		}
	}	
	else {
		qWarning() << "Something went wrong! Found rnn text component without lnn.";
	}

}

// WhiteSpace --------------------------------------------------------------------
WhiteSpace::WhiteSpace(){
	mBbox = Rect();
	mIsBCR = false;
	mIsWCC = false;
	mHasANN = false;
}

WhiteSpace::WhiteSpace(Rect r){
	mBbox = r;
	mIsBCR = false;
	mIsWCC = false;
	mHasANN = false;
}

Rect WhiteSpace::bbox() const{
	return mBbox;
}

bool WhiteSpace::isNull() const{
	return mBbox.isNull();
}

void WhiteSpace::setIsBCR(bool isBCR){
	mIsBCR = isBCR;
}

void WhiteSpace::setIsWCC(bool isWCC){
	mIsWCC = isWCC;
}

bool WhiteSpace::isBCR() const{
	return mIsBCR;
}

bool WhiteSpace::isWCC() const{
	return mIsWCC;
}

bool WhiteSpace::hasANN() const
{
	return mHasANN;
}

void WhiteSpace::setHasANN(bool HasANN){
	mHasANN = HasANN;
}

QVector<QSharedPointer<WhiteSpace>> WhiteSpace::bnn(){
	return mBnn;
}

void WhiteSpace::addBnn(const QSharedPointer<WhiteSpace> ws){
	mBnn.append(ws);
}

void WhiteSpace::setBnn(const QVector<QSharedPointer<WhiteSpace>> bnn){
	mBnn = bnn;
}

WSRun::WSRun(){

}

WSRun::WSRun(QSharedPointer<WhiteSpace> ws){
	mWhiteSpaces.append(ws);
}

void WSRun::appendAllBnn(const QSharedPointer<WhiteSpace> ws){
	
	//TODO append bnn only if it shares a common x span with the previous components

	if (!ws->bnn().isEmpty()) {

		if (ws->bnn().length() == 1) {
			mWhiteSpaces.append(ws);
			appendAllBnn(ws->bnn().at(0));
		}
		else {
			if (ws->bnn().length() > 1)
				return;
		}
	}
	else {
		mWhiteSpaces.append(ws);
	}
}

QVector<QSharedPointer<WhiteSpace>> WSRun::whiteSpaces(){
	return mWhiteSpaces;
}

void WSRun::setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>> whiteSpaces){
	mWhiteSpaces = whiteSpaces;
}

// TextBlockFormation --------------------------------------------------------------------
TextBlockFormation::TextBlockFormation(){
}

TextBlockFormation::TextBlockFormation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>> textLines){
	mImg = img;
	mTextLines = textLines;
}

bool TextBlockFormation::compute(){

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

	cv::Mat imgBf = draw(mImg);
	QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_blockFormation");
	Image::save(imgBf, imgPath);

	return true;
}

void TextBlockFormation::computeAdjacency(){

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

void TextBlockFormation::formTextBlocks(){

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

TextBlock TextBlockFormation::createTextBlock(const QVector<QSharedPointer<TextLineSet>>& lines){
	Rect bbox = Rect();
	PixelSet pSet;
	for (auto l : lines) {
		pSet.append(l->pixels());

		if (bbox.isNull())
			bbox = l->boundingBox();
		else {
			bbox = bbox.joined(l->boundingBox());
		}
	}

	TextBlock tb(Polygon::fromRect(bbox));
	tb.setTextLines(lines);
	tb.addPixels(pSet);

	return tb;
}

cv::Mat TextBlockFormation::draw(const cv::Mat & img, const QColor & col){

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

		QString bnnC = QString::number(bnnIndices[i].size());
		QString aC = QString::number(annCount[i]);
		painter.drawText(tlR.toQRect(), Qt::AlignCenter, "annCount = " + aC + ", bnnCount = " + bnnC);
	}

	return Image::qImage2Mat(qImg);
}

TextBlockSet TextBlockFormation::textBlockSet(){
	return mTextBlockSet;
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
	double ml = checkParam(mMinLineLength, 0, INT_MAX, "minLineLength");

	//ml *= ScaleFactory::scaleFactorDpi();
	return qRound(ml);
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

	QVector<Rect> mergedRects(textRects);
	QVector<int> processedIdx;

	for (int i = 0; i < textRects.size(); ++i) {
		if (processedIdx.contains(i))
			continue;

		for (int j = 0; j < mergedRects.size(); ++j) {

			if (i == j || processedIdx.contains(j))
				continue;

			Rect tr = textRects[i];
			Rect mr = mergedRects[j];

			if (mr.left() <= tr.right() && tr.left() <= mr.right()) {
				mergedRects[j] = mr.joined(tr);
				processedIdx << i;
			}
		}
	}

	QVector<Rect> mergedRects_final; //regions covered by text components
	for (int j = 0; j < mergedRects.size(); ++j) {
		if (processedIdx.contains(j))
			continue;

		mergedRects_final << mergedRects[j];
	}

	//sort text rects according to their x coordinates
	std::sort(mergedRects_final.begin(), mergedRects_final.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.right() < rhs.right();
	});

	QVector<QSharedPointer<WhiteSpacePixel>> whiteSpaces;

	for (int i = 1; i < mergedRects_final.size(); ++i) {
		Rect r1 = mergedRects_final[i - 1];
		Rect r2 = mergedRects_final[i];

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
	int minTextLineSize = 10;

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
	int updateCounter = 0;
	while (updatedSegmentation) {

		//update status of remaining BCR
		updateBCRStatus();
		
		////debugging
		//cv::Mat img_stlb = drawSplitTextLines(mImg);
		//cv::Mat img_wsr = drawWhiteSpaceRuns(mImg);

		//remove groups that represent non-bcr runs or are too short
		updatedSegmentation = refineWhiteSpaceRuns();
		
		////debugging
		//++updateCounter;
		//
		//QString imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_stl_"+ QString::number(updateCounter));
		//Image::save(img_stlb, imgPath);
		//imgPath = Utils::createFilePath("E:/data/test/HBR2013_training/debug.tif", "_wsr_" + QString::number(updateCounter));
		//Image::save(img_wsr, imgPath);
	}

	//TODO further refinement of text lines!?
	//refineTLC();

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
	QList<double> spacings;
	for (auto tl : mTlsM) {
		spacings << tl->lineSpacing();
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
	QImage qImg(img.size().width, img.size().height, QImage::Format_ARGB32);	//blank image
	//QImage qImg = Image::mat2QImage(img, true);
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

// Horizontal Pixel Connector --------------------------------------------------------------------

RightNNConnector::RightNNConnector() : PixelConnector() {
}

QVector<QSharedPointer<PixelEdge>> RightNNConnector::connect(const QVector<QSharedPointer<Pixel>>& pixels) const{

	//TODO check parameter choice

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
	QList<double> spacings;
	for (const QSharedPointer<Pixel>& px : pixels) {

		if (!px->stats()) {
			qWarning() << "stats is NULL where it should not be...";
			continue;
		}
		spacings << px->stats()->lineSpacing();
	}
	textWidthEstimate = (Algorithms::statMoment(spacings, 0.5))*0.5;


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