//
// Created by PinkySmile on 07/01/2023.
//

#ifndef SOKULOBBIES_SMALLHOSTLIST_HPP
#define SOKULOBBIES_SMALLHOSTLIST_HPP


#include <SokuLib.hpp>
#include <thread>
#include <mutex>

class SmallHostlist {
private:
	class Object {
	public:
		SokuLib::Vector2i translate{0, 0};

		virtual void update() = 0;
		virtual void render() = 0;
	};

	class Image : public Object {
	private:
		SokuLib::DrawUtils::Sprite &_sprite;
		SokuLib::Vector2i _pos;

	public:
		Image(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos);
		void update() override;
		void render() override;
	};

	class ScrollingImage : public Object {
	private:
		SokuLib::DrawUtils::Sprite &_sprite;
		SokuLib::Vector2i _startPos;
		SokuLib::Vector2i _endPos;
		unsigned _animationDuration;
		unsigned _animationCtr = 0;

	public:
		ScrollingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i startPos, SokuLib::Vector2i endPos, unsigned animationDuration);
		void update() override;
		void render() override;
	};

	class RotatingImage : public Object {
	private:
		SokuLib::DrawUtils::Sprite &_sprite;
		SokuLib::Vector2i _pos;
		float _anglePerFrame;
		float _rotation = 0;

	public:
		RotatingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos, float anglePerFrame);
		void update() override;
		void render() override;
	};

	struct HostEntry {
		std::string nameStr;
		SokuLib::DrawUtils::Sprite name;
		std::string msgStr;
		SokuLib::DrawUtils::Sprite msg;
		std::string country;
		std::string ip;
		unsigned short port;
		bool ap;
		bool ranked;
		bool deleted = false;
	};

	struct PlayEntry {
		std::string namesStr;
		SokuLib::DrawUtils::Sprite names;
		std::string p1chr;
		std::string p2chr;
		std::string country1;
		std::string country2;
		std::string ip;
		unsigned short port;
		bool deleted = false;
	};

	static constexpr const char *_spritesPaths[] = {
		"data/menu/connect/Network.png",
		"data/menu/door_l.png",
		"data/menu/door_r.png",
		"data/menu/gear/0L-Black_grad.png",
		"data/menu/gear/0U-Black_grad.png",
		"data/menu/gear/1L-rod_L.png",
		"data/menu/gear/1L-rod_S.png",
		"data/menu/gear/2L-front_L.png",
		"data/menu/gear/2L-front_M.png",
		"data/menu/gear/2L-front_S.png",
		"data/menu/gear/2U-front_L.png",
		"data/menu/gear/2U-front_M.png",
		"data/menu/gear/2U-front_S.png",
		"data/menu/gear/3L-frame.png",
		"data/menu/gear/3U-frame.png",
		"data/menu/gear/4L-mid_L.png",
		"data/menu/gear/4L-mid_M.png",
		"data/menu/gear/4L-mid_S.png",
		"data/menu/gear/4U-mid_M.png",
		"data/menu/gear/4U-mid_S.png",
		"data/menu/gear/5L-back_L.png",
		"data/menu/gear/5L-back_M.png",
		"data/menu/gear/5U-back_M.png",
		"data/menu/gear/6L_pattern.png",
		"data/menu/gear/6U_pattern.png"
	};
	SokuLib::DrawUtils::Sprite _hosting;
	SokuLib::DrawUtils::Sprite _playing;
	std::array<SokuLib::DrawUtils::Sprite, sizeof(_spritesPaths) / sizeof(*_spritesPaths) + 3> _sprites;
	std::vector<std::unique_ptr<Object>> _topOverlay;
	std::vector<std::unique_ptr<Object>> _botOverlay;
	std::vector<std::unique_ptr<Object>> _background;
	std::vector<std::unique_ptr<Object>> _foreground;
	std::thread _netThread;
	unsigned _overlayTimer = 0;
	bool _selection = false;
	bool _selected = false;
	bool _spectator = false;
	unsigned _hostSelect = 0;
	std::vector<std::unique_ptr<HostEntry>> _hostEntries;
	std::vector<std::unique_ptr<PlayEntry>> _playEntries;
	std::mutex _entriesMutex;
	SokuLib::MenuConnect *_parent;
	SokuLib::Vector2i _pos;
	float _ratio;

	void _displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size);
	void _refreshHostlist();

public:
	SmallHostlist(float ratio, SokuLib::Vector2i pos, SokuLib::MenuConnect *parent);
	~SmallHostlist();
	bool update();
	void render();
};


#endif //SOKULOBBIES_SMALLHOSTLIST_HPP
