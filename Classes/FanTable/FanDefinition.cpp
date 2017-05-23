﻿#include "FanDefinition.h"
#include "ui/UIWebView.h"
#include "../compiler.h"
#include "../TilesImage.h"
#include "../mahjong-algorithm/stringify.h"
#include "../mahjong-algorithm/fan_calculator.h"
#include "../widget/LoadingView.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS) && !defined(CC_PLATFORM_OS_TVOS)
#define HAS_WEBVIEW 1
#else
#define HAS_WEBVIEW 0
#endif

USING_NS_CC;

static std::vector<std::string> g_principles;
static std::vector<std::string> g_definitions;

static void replaceTilesToImage(std::string &text, float scale) {
    char tilesStr[128];
    mahjong::tile_t tiles[14];
    long tilesCnt;
    char imgStr[1024];

    std::string::size_type pos = text.find('[');
    while (pos != std::string::npos) {
        const char *str = text.c_str();
        int readLen;
        if (sscanf(str + pos + 1, "%[^]]%n", tilesStr, &readLen) != EOF
            && str[pos + readLen + 1] == ']'
            && (tilesCnt = mahjong::parse_tiles(tilesStr, tiles, 14)) >= 0) {
            size_t totalWriteLen = 0;
            for (long i = 0; i < tilesCnt; ++i) {
                int writeLen = snprintf(imgStr + totalWriteLen, sizeof(imgStr) - totalWriteLen,
                    "<img src=\"%s\" width=\"%d\" height=\"%d\"/>",
                    tilesImageName[tiles[i]], (int)(TILE_WIDTH * scale), (int)(TILE_HEIGHT * scale));
                totalWriteLen += writeLen;
            }
            text.replace(pos, readLen + 2, imgStr);
            pos = text.find('[', pos + totalWriteLen);
        }
        else {
            pos = text.find('[', pos + 1);
        }
    }
}

Scene *FanDefinitionScene::createScene(size_t idx) {
    auto scene = Scene::create();
    auto layer = new (std::nothrow) FanDefinitionScene();
    layer->initWithIndex(idx);
    layer->autorelease();

    scene->addChild(layer);
    return scene;
}

bool FanDefinitionScene::initWithIndex(size_t idx) {
    const char *title = idx < 100 ? mahjong::fan_name[idx] : principle_title[idx - 100];
    if (UNLIKELY(!BaseLayer::initWithTitle(title))) {
        return false;
    }

    if (LIKELY(!g_definitions.empty() && !g_principles.empty())) {
        createContentView(idx);
    }
    else {
        Size visibleSize = Director::getInstance()->getVisibleSize();
        Vec2 origin = Director::getInstance()->getVisibleOrigin();

        LoadingView *loadingView = LoadingView::create();
        this->addChild(loadingView);
        loadingView->setPosition(origin);

#if HAS_WEBVIEW
        float scale = 1.0f;
        float maxWidth = (visibleSize.width - 10) / 18;
        if (maxWidth < 25) {
            scale = maxWidth / TILE_WIDTH;
        }
#else
        float scale = 1.0f / Director::getInstance()->getContentScaleFactor();
#endif

        auto thiz = makeRef(this);  // 保证线程回来之前不析构
        std::thread([thiz, idx, scale, loadingView]() {
            ValueVector valueVec = FileUtils::getInstance()->getValueVectorFromFile("text/score_definition.xml");
            g_definitions.reserve(valueVec.size());
            std::transform(valueVec.begin(), valueVec.end(), std::back_inserter(g_definitions), [scale](const Value &value) {
                std::string ret = value.asString();
                replaceTilesToImage(ret, scale);
                return std::move(ret);
            });

            valueVec = FileUtils::getInstance()->getValueVectorFromFile("text/score_principles.xml");
            g_principles.reserve(valueVec.size());
            std::transform(valueVec.begin(), valueVec.end(), std::back_inserter(g_principles), [scale](const Value &value) {
                std::string ret = value.asString();
                replaceTilesToImage(ret, scale);
                return std::move(ret);
            });

            // 切换到cocos线程
            Director::getInstance()->getScheduler()->performFunctionInCocosThread([thiz, idx, loadingView]() {
                if (LIKELY(thiz->getParent() != nullptr)) {
                    loadingView->removeFromParent();
                    thiz->createContentView(idx);
                }
            });
        }).detach();
    }

    return true;
}

void FanDefinitionScene::createContentView(size_t idx) {
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    const std::string &text = idx < 100 ? g_definitions[idx] : g_principles[idx - 100];

#if HAS_WEBVIEW
    experimental::ui::WebView *webView = experimental::ui::WebView::create();
    webView->setContentSize(Size(visibleSize.width, visibleSize.height - 35));
    webView->setOnEnterCallback(std::bind(&experimental::ui::WebView::loadHTMLString, webView, text, ""));
    this->addChild(webView);
    webView->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + visibleSize.height * 0.5f - 15.0f));
#else
    ui::RichText *richText = ui::RichText::createWithXML("<font face=\"Verdana\" size=\"12\" color=\"#000000\">" + text + "</font>");
    richText->setContentSize(Size(visibleSize.width - 10, 0));
    richText->ignoreContentAdaptWithSize(false);
    richText->setVerticalSpace(2);
    richText->formatText();
    this->addChild(richText);
    richText->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + visibleSize.height - 40.0f));
#endif
}
