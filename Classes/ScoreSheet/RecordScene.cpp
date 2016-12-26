﻿#include "RecordScene.h"
#include "../common.h"
#include "../mahjong-algorithm/points_calculator.h"
#include "../widget/AlertLayer.h"
#include "../widget/CWTableView.h"

USING_NS_CC;

Scene *RecordScene::createScene(size_t handIdx, const char **playerNames, const Record::Detail *detail, const std::function<void (const Record::Detail &)> &okCallback) {
    auto scene = Scene::create();
    auto layer = new (std::nothrow) RecordScene();
    layer->initWithIndex(handIdx, playerNames, detail);
    layer->_okCallback = okCallback;
    layer->autorelease();

    scene->addChild(layer);
    return scene;
}

static const int pointsLevel[] = { 4, 6, 8, 12, 16, 24, 32, 48, 64, 88 };
static const size_t eachLevelBeginIndex[] =
#if HAS_CONCEALED_KONG_AND_MELDED_KONG
{ 56, 48, 39, 34, 28, 19, 16, 14, 8, 1 };
#else
{ 55, 48, 39, 34, 28, 19, 16, 14, 8, 1 };
#endif
static const size_t eachLevelCounts[] = { 4, 7, 8, 5, 6, 9, 3, 2, 6, 7 };  // 各档次的番种的个数

static inline size_t computeRowsAlign4(size_t cnt) {
    return (cnt >> 2) + !!(cnt & 0x3);
}

bool RecordScene::initWithIndex(size_t handIdx, const char **playerNames, const Record::Detail *detail) {
    if (!BaseLayer::initWithTitle(handNameText[handIdx])) {
        return false;
    }

    _winIndex = -1;
    if (detail != nullptr) {
        memcpy(&_detail, detail, sizeof(_detail));
    }
    else {
        memset(&_detail, 0, sizeof(_detail));
    }

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    // 番数输入框
    _editBox = ui::EditBox::create(Size(34.0f, 20.0f), ui::Scale9Sprite::create("source_material/btn_square_normal.png"));
    this->addChild(_editBox);
    _editBox->setInputFlag(ui::EditBox::InputFlag::SENSITIVE);
    _editBox->setInputMode(ui::EditBox::InputMode::NUMERIC);
    _editBox->setFontColor(Color4B::BLACK);
    _editBox->setFontSize(12);
    _editBox->setText("8");
    _editBox->setPosition(Vec2(origin.x + 65.0f, origin.y + visibleSize.height - 50));
    _editBox->setDelegate(this);

    // +-按钮
    ui::Button *minusButton = ui::Button::create("source_material/stepper_dec_n.png", "source_material/stepper_dec_h.png");
    this->addChild(minusButton);
    minusButton->setScale(20.0f / minusButton->getContentSize().height);
    minusButton->setAnchorPoint(Vec2::ANCHOR_MIDDLE_RIGHT);
    minusButton->setPosition(Vec2(origin.x + 48.0f, origin.y + visibleSize.height - 50));
    minusButton->addClickEventListener(std::bind(&RecordScene::onMinusButton, this, std::placeholders::_1));

    ui::Button *plusButton = ui::Button::create("source_material/stepper_inc_n.png", "source_material/stepper_inc_h.png");
    this->addChild(plusButton);
    plusButton->setScale(20.0f / plusButton->getContentSize().height);
    plusButton->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
    plusButton->setPosition(Vec2(origin.x + 82.0f, origin.y + visibleSize.height - 50));
    plusButton->addClickEventListener(std::bind(&RecordScene::onPlusButton, this, std::placeholders::_1));

    Label *label = Label::createWithSystemFont("番", "Arial", 12);
    this->addChild(label);
    label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
    label->setPosition(Vec2(origin.x + 120.0f, origin.y + visibleSize.height - 50));

    // 荒庄
    _drawBox = ui::CheckBox::create("source_material/btn_square_normal.png", "source_material/btn_square_highlighted.png");
    this->addChild(_drawBox);
    _drawBox->setZoomScale(0.0f);
    _drawBox->ignoreContentAdaptWithSize(false);
    _drawBox->setContentSize(Size(20.0f, 20.0f));
    _drawBox->setPosition(Vec2(origin.x + visibleSize.width - 60.0f, origin.y + visibleSize.height - 50));
    _drawBox->addEventListener(std::bind(&RecordScene::onDrawBox, this, std::placeholders::_1, std::placeholders::_2));

    label = Label::createWithSystemFont("荒庄", "Arial", 12);
    this->addChild(label);
    label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
    label->setPosition(Vec2(origin.x + visibleSize.width - 45.0f, origin.y + visibleSize.height - 50));

    _winGroup = ui::RadioButtonGroup::create();
    _winGroup->setAllowedNoSelection(true);
    this->addChild(_winGroup);
    _winGroup->addEventListener(std::bind(&RecordScene::onWinGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    _claimGroup = ui::RadioButtonGroup::create();
    _claimGroup->setAllowedNoSelection(true);
    this->addChild(_claimGroup);
    _claimGroup->addEventListener(std::bind(&RecordScene::onClaimGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    const float gap = (visibleSize.width - 4.0f) * 0.25f;
    for (int i = 0; i < 4; ++i) {
        const float x = origin.x + gap * (i + 0.5f);

        // 名字
        Label *nameLabel = Label::createWithSystemFont(playerNames[i], "Arial", 12);
        nameLabel->setColor(Color3B::YELLOW);
        this->addChild(nameLabel);
        nameLabel->setPosition(Vec2(x, origin.y + visibleSize.height - 80));
        scaleLabelToFitWidth(nameLabel, gap - 4);

        // 得分
        _scoreLabel[i] = Label::createWithSystemFont("+0", "Arial", 12);
        _scoreLabel[i]->setColor(Color3B::GRAY);
        this->addChild(_scoreLabel[i]);
        _scoreLabel[i]->setPosition(Vec2(x, origin.y + visibleSize.height - 105));

        // 和
        ui::RadioButton *button = ui::RadioButton::create("source_material/btn_square_normal.png", "", "source_material/btn_square_highlighted.png",
            "source_material/btn_square_disabled.png", "source_material/btn_square_disabled.png");
        this->addChild(button);
        button->setZoomScale(0.0f);
        button->ignoreContentAdaptWithSize(false);
        button->setContentSize(Size(20.0f, 20.0f));
        button->setPosition(Vec2(x - 15, origin.y + visibleSize.height - 130));
        _winGroup->addRadioButton(button);

        label = Label::createWithSystemFont("和", "Arial", 12);
        this->addChild(label);
        label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
        label->setPosition(Vec2(x, origin.y + visibleSize.height - 130));

        // 点炮或自摸
        button = ui::RadioButton::create("source_material/btn_square_normal.png", "", "source_material/btn_square_highlighted.png",
            "source_material/btn_square_disabled.png", "source_material/btn_square_disabled.png");
        this->addChild(button);
        button->setZoomScale(0.0f);
        button->ignoreContentAdaptWithSize(false);
        button->setContentSize(Size(20.0f, 20.0f));
        button->setPosition(Vec2(x - 15, origin.y + visibleSize.height - 160));
        _claimGroup->addRadioButton(button);

        label = Label::createWithSystemFont("点炮", "Arial", 12);
        this->addChild(label);
        label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
        label->setPosition(Vec2(x, origin.y + visibleSize.height - 160));
        _byDiscardLabel[i] = label;

        label = Label::createWithSystemFont("自摸", "Arial", 12);
        this->addChild(label);
        label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
        label->setPosition(Vec2(x, origin.y + visibleSize.height - 160));
        _selfDrawnLabel[i] = label;
        label->setVisible(false);

        // 错和
        _falseWinBox[i] = ui::CheckBox::create("source_material/btn_square_normal.png", "", "source_material/btn_square_highlighted.png",
            "source_material/btn_square_disabled.png", "source_material/btn_square_disabled.png");
        this->addChild(_falseWinBox[i]);
        _falseWinBox[i]->setZoomScale(0.0f);
        _falseWinBox[i]->ignoreContentAdaptWithSize(false);
        _falseWinBox[i]->setContentSize(Size(20.0f, 20.0f));
        _falseWinBox[i]->setPosition(Vec2(x - 15, origin.y + visibleSize.height - 190));
        _falseWinBox[i]->addEventListener(std::bind(&RecordScene::onFalseWinBox, this, std::placeholders::_1, std::placeholders::_2));

        label = Label::createWithSystemFont("错和", "Arial", 12);
        this->addChild(label);
        label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
        label->setPosition(Vec2(x, origin.y + visibleSize.height - 190));
    }

    // 说明
    label = Label::createWithSystemFont("标记番种（未做排斥检测）", "Arial", 12);
    this->addChild(label);
    label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
    label->setPosition(Vec2(origin.x + 5.0f, origin.y + visibleSize.height - 220));

    label = Label::createWithSystemFont("标记番种可快速增加番数，取消标记不减少。", "Arial", 10);
    this->addChild(label);
    label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
    label->setPosition(Vec2(origin.x + 5.0f, origin.y + visibleSize.height - 240));

    cw::TableView *tableView = cw::TableView::create();
    tableView->setContentSize(Size(visibleSize.width - 10, visibleSize.height - 300));
    tableView->setTableViewCallback([this](cw::TableView *table, cw::TableView::CallbackType type, intptr_t param)->intptr_t {
        switch (type) {
        case cw::TableView::CallbackType::CELL_SIZE: {
            auto p = (cw::TableView::CellSizeParam *)param;
            size_t cnt = eachLevelCounts[p->idx];
            float height = computeRowsAlign4(cnt) * 25.0f;
            p->size = Size(0, height + 15.0f);
            return 0;
        }
        case cw::TableView::CallbackType::CELL_AT_INDEX:
            return (intptr_t)tableCellAtIndex(table, param);
        case cw::TableView::CallbackType::NUMBER_OF_CELLS:
            return (intptr_t)10;
        }
        return 0;
    });

    tableView->setDirection(ui::ScrollView::Direction::VERTICAL);
    tableView->setVerticalFillOrder(cw::TableView::VerticalFillOrder::TOP_DOWN);

    tableView->setScrollBarPositionFromCorner(Vec2(5, 5));
    tableView->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    tableView->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + visibleSize.height * 0.5f - 110.0f));
    tableView->reloadData();
    this->addChild(tableView);

    // 确定按钮
    _okButton = ui::Button::create("source_material/btn_square_normal.png", "source_material/btn_square_highlighted.png", "source_material/btn_square_disabled.png");
    this->addChild(_okButton);
    _okButton->setScale9Enabled(true);
    _okButton->setContentSize(Size(52.0f, 22.0f));
    _okButton->setTitleText("确定");
    _okButton->setTitleColor(Color3B::BLACK);
    _okButton->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + 15));
    _okButton->addClickEventListener(std::bind(&RecordScene::onOkButton, this, std::placeholders::_1));
    _okButton->setEnabled(false);

    if (detail != nullptr) {
        refresh();
    }
    return true;
}

cw::TableViewCell *RecordScene::tableCellAtIndex(cw::TableView *table, ssize_t idx) {
    typedef cw::TableViewCellEx<Label *, ui::Button *[9]> CustomCell;
    CustomCell *cell = (CustomCell *)table->dequeueCell();

    if (cell == nullptr) {
        cell = CustomCell::create();

        CustomCell::ExtDataType &ext = cell->getExtData();
        Label *&label = std::get<0>(ext);
        ui::Button *(&buttons)[9] = std::get<1>(ext);

        label = Label::createWithSystemFont("1番", "Arial", 12);
        label->setAnchorPoint(Vec2::ANCHOR_MIDDLE_LEFT);
        cell->addChild(label);

        for (size_t k = 0; k < 9; ++k) {
            size_t idx0 = eachLevelBeginIndex[idx] + k;
            ui::Button *button = ui::Button::create("source_material/btn_square_normal.png", "source_material/btn_square_highlighted.png");
            button->setScale9Enabled(true);
            button->setContentSize(Size(65.0f, 20.0f));
            button->setTitleColor(Color3B::BLACK);
            button->setTitleFontSize(12);
            button->addClickEventListener(std::bind(&RecordScene::onPointsNameButton, this, std::placeholders::_1));

            cell->addChild(button);
            buttons[k] = button;
        }
    }

    const size_t currentLevelCount = eachLevelCounts[idx];

    Size visibleSize = Director::getInstance()->getVisibleSize();
    const float gap = (visibleSize.width - 10.0f) * 0.25f;
    size_t totalRows = computeRowsAlign4(currentLevelCount);

    const CustomCell::ExtDataType &ext = cell->getExtData();
    Label *label = std::get<0>(ext);
    ui::Button *const (&buttons)[9] = std::get<1>(ext);

    label->setString(StringUtils::format("%d番", pointsLevel[idx]));
    label->setPosition(Vec2(5.0f, totalRows * 25.0f + 7.0f));

    for (size_t k = 0; k < currentLevelCount; ++k) {
        size_t idx0 = eachLevelBeginIndex[idx] + k;
        ui::Button *button = buttons[k];
        button->setTitleText(mahjong::points_name[idx0]);
        button->setTag(idx0);
        button->setVisible(true);
        button->setEnabled(true);

        size_t col = k & 0x3;
        size_t row = k >> 2;
        button->setPosition(Vec2(gap * (col + 0.5f), (totalRows - row - 0.5f) * 25.0f));

#if HAS_CONCEALED_KONG_AND_MELDED_KONG
        if (idx0 > mahjong::POINT_TYPE::CONCEALED_KONG_AND_MELDED_KONG) {
            --idx0;
        }
#endif
        bool selected = !!(_detail.points_flag & (1ULL << idx0));
        button->setHighlighted(selected);
        button->setUserData((void *)selected);
    }

    for (size_t k = currentLevelCount; k < 9; ++k) {
        buttons[k]->setVisible(false);
        buttons[k]->setEnabled(false);
    }

    return cell;
}

void RecordScene::editBoxReturn(cocos2d::ui::EditBox *editBox) {
    updateScoreLabel();
}

void RecordScene::refresh() {
    int wc = _detail.win_claim;
    if (_detail.score >= 8) {
        char str[32];
        snprintf(str, sizeof(str), "%d", _detail.score);
        _editBox->setText(str);
    }

    _winIndex = (wc & 0x80) ? 3 : (wc & 0x40) ? 2 : (wc & 0x20) ? 1 : (wc & 0x10) ? 0 : -1;
    if (_winIndex != -1) {  // 有人和牌
        int claimIndex = (wc & 0x8) ? 3 : (wc & 0x4) ? 2 : (wc & 0x2) ? 1 : (wc & 0x1) ? 0 : -1;  // 点炮者
        _winGroup->setSelectedButton(_winIndex);
        if (claimIndex != -1) {
            _claimGroup->setSelectedButton(claimIndex);
        }
    }

    // 错和
    if (_detail.false_win != 0) {
        for (int i = 0; i < 4; ++i) {
            _falseWinBox[i]->setSelected(!!(_detail.false_win & (1 << i)));
        }
    }

    if (_detail.score == 0) {
        _drawBox->setSelected(true);
        onDrawBox(_drawBox, ui::CheckBox::EventType::SELECTED);
    }
    else {
        updateScoreLabel();
    }
}

void RecordScene::updateScoreLabel() {
    _detail.win_claim = 0;
    int claimIndex = -1;
    if (_winIndex != -1) {  // 有人和牌
        int winScore = atoi(_editBox->getText());  // 获取输入框里所填番数
        _detail.score = std::max(8, winScore);
        claimIndex = _claimGroup->getSelectedButtonIndex();

        // 记录和牌和点炮
        _detail.win_claim = (1 << (_winIndex + 4));
        if (claimIndex != -1) {
            _detail.win_claim |= (1 << claimIndex);
        }
    }
    else {  // 荒庄
        _detail.score = 0;
    }

    // 检查是否有错和
    _detail.false_win = 0;
    for (int i = 0; i < 4; ++i) {
        if (_falseWinBox[i]->isEnabled() && _falseWinBox[i]->isSelected()) {
            _detail.false_win |= (1 << i);
        }
    }

    int scoreTable[4];
    translateDetailToScoreTable(_detail, scoreTable);

    // 正负0分使用不同颜色
    for (int i = 0; i < 4; ++i) {
        _scoreLabel[i]->setString(StringUtils::format("%+d", scoreTable[i]));
        if (scoreTable[i] > 0) {
            _scoreLabel[i]->setColor(Color3B::RED);
        }
        else if (scoreTable[i] < 0) {
            _scoreLabel[i]->setColor(Color3B::GREEN);
        }
        else {
            _scoreLabel[i]->setColor(Color3B::GRAY);
        }
    }

    // 四位选手的总分加起来和为0
    if (scoreTable[0] + scoreTable[1] + scoreTable[2] + scoreTable[3] == 0) {
        if (_drawBox->isSelected()) {  // 荒庄
            _okButton->setEnabled(true);
        }
        else {
            _okButton->setEnabled(_winGroup->getSelectedButtonIndex() != -1);
        }
    }
    else {
        // 四位选手的总分加起来和不为0说明还没有选择是自摸还是点炮
        _okButton->setEnabled(false);
    }
}

void RecordScene::onMinusButton(cocos2d::Ref *sender) {
    int winScore = atoi(_editBox->getText());
    if (winScore > 8) {
        --winScore;
        _editBox->setText(StringUtils::format("%d", winScore).c_str());
        updateScoreLabel();
    }
}

void RecordScene::onPlusButton(cocos2d::Ref *sender) {
    int winScore = atoi(_editBox->getText());
    ++winScore;
    _editBox->setText(StringUtils::format("%d", winScore).c_str());
    updateScoreLabel();
}

void RecordScene::onDrawBox(cocos2d::Ref *sender, cocos2d::ui::CheckBox::EventType event) {
    if (event == ui::CheckBox::EventType::SELECTED) {
        _winIndex = -1;
        // 禁用所有人的和、自摸/点炮，启用错和
        for (int i = 0; i < 4; ++i) {
            ui::RadioButton *button = _winGroup->getRadioButtonByIndex(i);
            button->setEnabled(false);
            button = _claimGroup->getRadioButtonByIndex(i);
            button->setEnabled(false);
            _falseWinBox[i]->setEnabled(true);
        }

    }
    else {
        _winIndex = _winGroup->getSelectedButtonIndex();
        // 启用所有人的和、自摸/点炮、错和
        for (int i = 0; i < 4; ++i) {
            ui::RadioButton *button = _winGroup->getRadioButtonByIndex(i);
            button->setEnabled(true);
            button = _claimGroup->getRadioButtonByIndex(i);
            button->setEnabled(true);
            _falseWinBox[i]->setEnabled(i != _winIndex);
        }

    }
    updateScoreLabel();
}

void RecordScene::onWinGroup(cocos2d::ui::RadioButton *radioButton, int index, cocos2d::ui::RadioButtonGroup::EventType event) {
    _winIndex = index;
    for (int i = 0; i < 4; ++i) {
        if (i == index) {
            // 和的选手，显示自摸，禁用错和
            _byDiscardLabel[i]->setVisible(false);
            _selfDrawnLabel[i]->setVisible(true);
            _falseWinBox[i]->setEnabled(false);
        }
        else {
            // 未和的选手，显示点炮，启用错和
            _byDiscardLabel[i]->setVisible(true);
            _selfDrawnLabel[i]->setVisible(false);
            _falseWinBox[i]->setEnabled(true);
        }
    }
    updateScoreLabel();
}

void RecordScene::onClaimGroup(cocos2d::ui::RadioButton *radioButton, int index, cocos2d::ui::RadioButtonGroup::EventType event) {
    if (_winIndex == -1) return;
    updateScoreLabel();
}

void RecordScene::onFalseWinBox(cocos2d::Ref *sender, cocos2d::ui::CheckBox::EventType event) {
    updateScoreLabel();
}

void RecordScene::onPointsNameButton(cocos2d::Ref *sender) {
    ui::Button *button = (ui::Button *)sender;
    int index = button->getTag();
#if HAS_CONCEALED_KONG_AND_MELDED_KONG
    if (index > mahjong::POINT_TYPE::CONCEALED_KONG_AND_MELDED_KONG) {
        --index;
    }
#endif
    // 标记/取消标记番种
    bool selected = !!button->getUserData();
    if (selected) {
        button->setHighlighted(false);
        button->setUserData((void *)false);
        _detail.points_flag &= ~(1ULL << index);
    }
    else {
        button->setHighlighted(true);
        button->setUserData((void *)true);
        _detail.points_flag |= 1ULL << index;
    }

    // 增加番数
    int prevWinScore = atoi(_editBox->getText());
    int currentWinScore = 0;
    for (int n = 0; n < 64; ++n) {
        if (_detail.points_flag & (1ULL << n)) {
            unsigned idx = n;
#if HAS_CONCEALED_KONG_AND_MELDED_KONG
            if (idx >= mahjong::POINT_TYPE::CONCEALED_KONG_AND_MELDED_KONG) {
                ++idx;
            }
#endif
            currentWinScore += mahjong::points_value_table[idx];
        }
    }
    currentWinScore = std::max(8, currentWinScore);
    if (currentWinScore > prevWinScore) {
        char str[16];
        snprintf(str, sizeof(str), "%d", currentWinScore);
        _editBox->setText(str);
    }
    updateScoreLabel();
}

void RecordScene::onOkButton(cocos2d::Ref *sender) {
    if (_drawBox->isSelected() && _detail.points_flag != 0) {
        AlertLayer::showWithMessage("记分", "你标记了番种却选择了荒庄，是否忽略标记这些番种，记录本盘为荒庄？", [this]() {
            _detail.points_flag = 0;
            _okCallback(_detail);
            Director::getInstance()->popScene();
        }, nullptr);
        return;
    }

    _okCallback(_detail);
    Director::getInstance()->popScene();
}
