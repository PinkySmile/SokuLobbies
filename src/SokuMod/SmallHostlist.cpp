//
// Created by PinkySmile on 07/01/2023.
//

#define _USE_MATH_DEFINES
#include <filesystem>
#include "data.hpp"
#include "SmallHostlist.hpp"

#define MAX_OVERLAY_ANIMATION 15
#define modifyPos(x, y) ((SokuLib::Vector2i{(x), (y)} * ratio + pos).to<int>())

SmallHostlist::SmallHostlist(float ratio, SokuLib::Vector2i pos) :
	_ratio(ratio)
{
	for (unsigned i = 0; i < this->_sprites.size(); i++) {
		auto &sprite = this->_sprites[i];

		if (i == this->_sprites.size() - 2) {
			sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/CRTeffect.png").string().c_str());
			sprite.setSize(sprite.texture.getSize());
		} else {
			if (i == this->_sprites.size() - 1)
				sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/title.png").string().c_str());
			else
				sprite.texture.loadFromGame(_spritesPaths[i]);
			sprite.setSize((sprite.texture.getSize() * ratio).to<unsigned>());
		}
		sprite.rect.width = sprite.texture.getSize().x;
		sprite.rect.height = sprite.texture.getSize().y;
	}
	this->_sprites[1].tint = SokuLib::Color{0x40, 0x40, 0x40, 0xFF};
	this->_sprites[2].tint = SokuLib::Color{0x40, 0x40, 0x40, 0xFF};
	this->_background.emplace_back(new Image(this->_sprites.back(), modifyPos(0, 0)));
	this->_background.emplace_back(new ScrollingImage(this->_sprites[1], modifyPos(-480, 0), modifyPos(0, 0), MAX_OVERLAY_ANIMATION));
	this->_background.emplace_back(new ScrollingImage(this->_sprites[2], modifyPos(640, 0), modifyPos(160, 0), MAX_OVERLAY_ANIMATION));

	this->_foreground.emplace_back(new Image(this->_sprites[0], modifyPos(15, 15)));
	this->_foreground.emplace_back(new Image(this->_sprites[this->_sprites.size() - 2], modifyPos(0, 0)));

	this->_topOverlay.emplace_back(new Image(this->_sprites[24], modifyPos(0, -200)));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[22], modifyPos(203, -270), -0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[22], modifyPos(512, -265), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(177, -205), 0.006981317007977318));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(245, -233), -0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(372, -199), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[18], modifyPos(24, -267), -0.003490658503988659));
	this->_topOverlay.emplace_back(new Image(this->_sprites[14], modifyPos(0, -200)));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(419, -236), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(162, -206), 0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(187, -216), -0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(191, -190), 0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(217, -194), -0.006981317007977318));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(307, -206), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(546, -237), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(451, -204), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(362, -224), -0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(572, -184), -0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(-47, -223), 0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(540, -216), -0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(275, -238), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(40, -250), -0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(-15, -191), 0.005235987755982988));
	this->_topOverlay.emplace_back(new Image(this->_sprites[4], modifyPos(0, -200)));

	this->_botOverlay.emplace_back(new Image(this->_sprites[23], modifyPos(0, 616)));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[21], modifyPos(335, 626), -0.003490658503988659));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[21], modifyPos(490, 618), 0.003490658503988659));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[20], modifyPos(54, 609), 0.0017453292519943296));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(-26, 607), 0.006981317007977318));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(559, 603), -0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(632, 593), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[16], modifyPos(419, 619), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[15], modifyPos(6, 611), -0.0017453292519943296));
	this->_botOverlay.emplace_back(new Image(this->_sprites[13], modifyPos(0, 612)));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(435, 636), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(151, 613), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(-37, 626), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(363, 636), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(285, 622), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(467, 668), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(337, 663), -0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(262, 620), -0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(109, 658), 0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(83, 647), -0.020943951023931952));
	this->_botOverlay.emplace_back(new Image(this->_sprites[3], modifyPos(0, 632)));
}

void SmallHostlist::update()
{
	if (this->_overlayTimer <= MAX_OVERLAY_ANIMATION) {
		int translate = (200 * this->_ratio) * std::pow((float)this->_overlayTimer / MAX_OVERLAY_ANIMATION, 2);

		if (this->_overlayTimer == 5)
			SokuLib::playSEWaveBuffer(61);
		for (auto &elem : this->_topOverlay)
			elem->translate.y = translate;
		for (auto &elem : this->_botOverlay)
			elem->translate.y = 1 - translate;
		this->_overlayTimer++;
	}
	for (auto &elem : this->_background)
		elem->update();
	for (auto &elem : this->_topOverlay)
		elem->update();
	for (auto &elem : this->_botOverlay)
		elem->update();
	if (this->_overlayTimer <= 5)
		return;
	for (auto &elem : this->_foreground)
		elem->update();
}

void SmallHostlist::render()
{
	for (auto &elem : this->_background)
		elem->render();
	for (auto &elem : this->_topOverlay)
		elem->render();
	for (auto &elem : this->_botOverlay)
		elem->render();
	if (this->_overlayTimer <= 5)
		return;
	for (auto &elem : this->_foreground)
		elem->render();
}

SmallHostlist::Image::Image(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos) :
	_sprite(sprite),
	_pos(pos)
{
}

void SmallHostlist::Image::update()
{
}

void SmallHostlist::Image::render()
{
	this->_sprite.setPosition(this->_pos + this->translate);
	this->_sprite.draw();
}

SmallHostlist::ScrollingImage::ScrollingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i startPos, SokuLib::Vector2i endPos, unsigned int animationDuration) :
	_sprite(sprite),
	_startPos(startPos),
	_endPos(endPos),
	_animationDuration(animationDuration)
{
}

void SmallHostlist::ScrollingImage::update()
{
	if (this->_animationDuration <= this->_animationCtr)
		return;
	this->_animationCtr++;
}

void SmallHostlist::ScrollingImage::render()
{
	this->_sprite.setPosition(this->_startPos + (this->_endPos - this->_startPos) * std::pow((float)this->_animationCtr / this->_animationDuration, 2) + this->translate);
	this->_sprite.draw();
}

SmallHostlist::RotatingImage::RotatingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos, float anglePerFrame) :
	_sprite(sprite),
	_pos(pos),
	_anglePerFrame(anglePerFrame)
{

}

void SmallHostlist::RotatingImage::update()
{
	this->_rotation = std::fmod(this->_rotation + this->_anglePerFrame, 2 * M_PI);
}

void SmallHostlist::RotatingImage::render()
{
	this->_sprite.setPosition(this->_pos + this->translate);
	this->_sprite.setRotation(this->_rotation);
	this->_sprite.draw();
}
