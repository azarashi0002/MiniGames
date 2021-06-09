
#include<Siv3D.hpp>
#include<HamFramework.hpp>

namespace Yeah {
	namespace Scenes { class IScene; }
	namespace Transitions { class ITransition; }
	class SceneChanger;

	namespace Scenes {

		class IScene {
			friend class SceneChanger;

			Optional<std::pair<std::unique_ptr<IScene>, std::unique_ptr<Transitions::ITransition>>> change_;
			bool exit_ = false;
			Optional<std::unique_ptr<Transitions::ITransition>> back_;
		public:
			virtual ~IScene() {}
			virtual void update() = 0;
			virtual void draw() const = 0;

			virtual void updateFadeIn(double /*t*/) { update(); }
			virtual void updateFadeOut(double /*t*/) { update(); }
			virtual void drawFadeIn(double /*t*/) const { draw(); }
			virtual void drawFadeOut(double /*t*/) const { draw(); }

			void changeScene(std::unique_ptr<IScene>&& scene,
				std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
				change_ = std::make_pair(std::move(scene), std::move(transition));
			}

			void exit() {
				exit_ = true;
			}
			void back(std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
				back_ = std::move(transition);
			}
		};
	}

	namespace Transitions {

		class ITransition {
		public:
			virtual ~ITransition() = default;
			virtual void update(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) = 0;
			virtual void draw(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const = 0;

			virtual Optional<std::unique_ptr<ITransition>> nextTransition() const = 0;
		};

		class Step :public ITransition {
		public:
			void update([[maybe_unused]] const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) override {
				if (after) {
					after->update();
				}
			}
			void draw([[maybe_unused]] const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const override {
				if (after) {
					after->draw();
				}
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				return none;
			}
		};

		class AlphaFadeOut :public ITransition {
			Timer timer;
		public:
			AlphaFadeOut(const Duration& fadeOutTime) :
				timer(fadeOutTime, true) {}
			void update(const std::unique_ptr<Scenes::IScene>& before,
				[[maybe_unused]] const std::unique_ptr<Scenes::IScene>& after) override {
				if (before) {
					const ScopedColorMul2D s(1.0, timer.progress1_0());
					before->updateFadeOut(timer.progress1_0());
				}
			}
			void draw(const std::unique_ptr<Scenes::IScene>& before,
				[[maybe_unused]] const std::unique_ptr<Scenes::IScene>& after) const override {
				if (before) {
					const ScopedColorMul2D s(1.0, timer.progress1_0());
					before->drawFadeOut(timer.progress1_0());
				}
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				if (timer.reachedZero()) {
					return Optional<std::unique_ptr<ITransition>>(std::make_unique<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};

		class AlphaFadeIn :public ITransition {
			Timer timer;
		public:
			AlphaFadeIn(const Duration& fadeInTime) :
				timer(fadeInTime, true) {}
			void update([[maybe_unused]] const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) override {
				if (after) {
					const ScopedColorMul2D s(1.0, timer.progress0_1());
					after->updateFadeIn(timer.progress0_1());
				}
			}
			void draw([[maybe_unused]] const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const override {
				if (after) {
					const ScopedColorMul2D s(1.0, timer.progress0_1());
					after->drawFadeIn(timer.progress0_1());
				}
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				if (timer.reachedZero()) {
					return Optional<std::unique_ptr<ITransition>>(std::make_unique<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};

		class AlphaFadeInOut :public ITransition {
			Timer timer;
			const Duration fadeOutTime, fadeInTime;
		public:
			AlphaFadeInOut(const Duration& fadeOutTime, const Duration& fadeInTime) :
				timer(fadeOutTime + fadeInTime, true),
				fadeInTime(fadeInTime),
				fadeOutTime(fadeOutTime) {}
			void update(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) override {
				if (timer.remaining() < fadeInTime) {
					if (after) {
						const ScopedColorMul2D s(1.0, (fadeInTime - timer.remaining()) / fadeInTime);
						after->updateFadeIn((fadeInTime - timer.remaining()) / fadeInTime);
					}
				}
				else {
					if (before) {
						const ScopedColorMul2D s(1.0, (timer.remaining() - fadeInTime) / fadeOutTime);
						before->updateFadeOut((timer.remaining() - fadeInTime) / fadeOutTime);
					}
				}
			}
			void draw(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const override {
				if (timer.remaining() < fadeInTime) {
					if (after) {
						const ScopedColorMul2D s(1.0, (fadeInTime - timer.remaining()) / fadeInTime);
						after->drawFadeIn((fadeInTime - timer.remaining()) / fadeInTime);
					}
				}
				else {
					if (before) {
						const ScopedColorMul2D s(1.0, (timer.remaining() - fadeInTime) / fadeOutTime);
						before->drawFadeOut((timer.remaining() - fadeInTime) / fadeOutTime);
					}
				}
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				if (timer.reachedZero()) {
					return Optional<std::unique_ptr<ITransition>>(std::make_unique<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};

		class CrossFade :public ITransition {
			Timer timer;
		public:
			CrossFade(const Duration& fadeTime) :
				timer(fadeTime, true) {}
			void update(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) override {
				if (after) {
					const ScopedColorMul2D mul(1.0, timer.progress0_1());
					after->updateFadeIn(timer.progress0_1());
				}
				if (before) {
					const ScopedColorMul2D mul(1.0, timer.progress1_0());
					before->updateFadeOut(timer.progress1_0());
				}
			}
			void draw(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const override {
				if (after) {
					const ScopedColorMul2D mul(1.0, timer.progress0_1());
					after->drawFadeIn(timer.progress0_1());
				}
				if (before) {
					const ScopedColorMul2D mul(1.0, timer.progress1_0());
					before->drawFadeOut(timer.progress1_0());
				}
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				if (timer.reachedZero()) {
					return Optional<std::unique_ptr<ITransition>>(std::make_unique<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};

	}

	class SceneChanger {
		Array<std::unique_ptr<Scenes::IScene>> scenes_, poped_scenes_;
		std::unique_ptr<Transitions::ITransition> transition_ = std::make_unique<Transitions::CrossFade>(1s);
	public:
		void change(std::unique_ptr<Scenes::IScene>&& next) {
			if (not next) { return; }

			scenes_ << std::move(next);
		}
		void change(std::unique_ptr<Scenes::IScene>&& next, std::unique_ptr<Transitions::ITransition>&& transition) {
			setTransition(std::move(transition));
			change(std::move(next));
		}
		void setTransition(std::unique_ptr<Transitions::ITransition>&& transition) {
			if (not transition) { return; }

			transition_ = std::move(transition);
		}

		void back(std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
			setTransition(std::move(transition));
			scenes_.pop_back();
		}

		bool update() {
			if (transition_) {
				transition_->update(before(), after());
			}

			if (after()){
				if (after()->change_) {
					change(std::move(after()->change_->first), std::move(after()->change_->second));
					//before()->change_.reset();
					after()->change_.reset();
				}
				if (after()->back_) {
					back(std::move(*after()->back_));
					after()->back_.reset();
				}
			}

			if (transition_) {
				if (auto&& i = transition_->nextTransition(); i.has_value()) {
					setTransition(std::move(*i));
				}
			}

			return after() ? not after()->exit_ : true;
		}
		void draw() const {
			if (transition_) {
				transition_->draw(before(), after());
			}

			Print << U"Transition:" << (transition_ ? Unicode::Widen(typeid(*transition_).name()).split(U':').back() : U"Null");
			Print << U"Before:" << (before() ? Unicode::Widen(typeid(*before()).name()) : U"Null");
			Print << U"After:" << (after() ? Unicode::Widen(typeid(*after()).name()) : U"Null");
		}

	private:
		std::unique_ptr<Yeah::Scenes::IScene>& before() {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return scenes_.size() >= 2 ? scenes_[scenes_.size() - 2] : nul;
		}
		const std::unique_ptr<Yeah::Scenes::IScene>& before() const {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return scenes_.size() >= 2 ? scenes_[scenes_.size() - 2] : nul;
		}
		std::unique_ptr<Yeah::Scenes::IScene>& after() {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return not scenes_.empty() ? scenes_.back() : nul;
		}
		const std::unique_ptr<Yeah::Scenes::IScene>& after() const {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return not scenes_.empty() ? scenes_.back() : nul;
		}
	};
}

class SceneFuctory {
public:
	template<typename T, typename...Args>
	static std::unique_ptr<Yeah::Scenes::IScene> Create(Args&&...args);
};
SceneFuctory sceneFuctory;
class TransitionFuctory {
public:
	template<typename T, typename...Args>
	static std::unique_ptr<Yeah::Transitions::ITransition> Create(Args&&...args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
};

namespace Master {
	class Title;
}
namespace FindShape {
	class Title;
	class GameScene1;
	class GameScene2;
	class Result;
	class Answer;
}
namespace CountFace {
	class Title;
}

namespace Master {
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"図形探し", { 400,350 }, 200)) {
				changeScene(SceneFuctory::Create<FindShape::Title>(), TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}
			if (SimpleGUI::ButtonAt(U"クソゲー2", { 400,400 }, 200)) {
				changeScene(SceneFuctory::Create<CountFace::Title>(), TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}
			if (SimpleGUI::ButtonAt(U"クソゲー3", { 400,450 }, 200)) {

			}
			if (SimpleGUI::ButtonAt(U"クソゲー4", { 400,500 }, 200)) {
				exit();
			}
		}
		void draw() const override {
			font(U"Title!!!!!!").drawAt({ 400,180 });
		}
	};
}
namespace FindShape {
	int32 shapenum;
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"イージー", { 400,350 }, 200)) {
				shapenum = 50;
				changeScene(
					SceneFuctory::Create<GameScene1>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ノーマル", { 400,400 }, 200)) {
				shapenum = 100;
				changeScene(
					SceneFuctory::Create<GameScene1>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ハード", { 400,450 }, 200)) {
				shapenum = 200;
				changeScene(
					SceneFuctory::Create<GameScene1>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFuctory::Create<Master::Title>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(U"図形を探せ！").drawAt({ 400,180 });
		}
	};

	struct Shape {
		Polygon polygon;
		ColorF color;
	};
	Array<Shape> shapes;
	int32 target_index;
	Optional<int32> grab_index;	//クリック
	Optional<int32> hold_index;	//0.1秒以上ホールド
	class GameScene1 :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		Timer timer{ 3s,true };
	public:
		GameScene1() {
			shapes.clear();
			for (int i : step(shapenum)) {
				const Vec2 center = RandomVec2(Rect(0, 0, 800, 600));
				const double angle = Random(Math::TwoPi);
				const ColorF color = RandomColorF();
				switch (Random(3)) {
				case 0:
				{
					const Triangle triangle(center, 100);
					shapes << Shape{ triangle.rotatedAt(center,angle).asPolygon(),color };
					break;
				}
				case 1:
				{
					const RectF rect(Arg::center = center, 60, 80);
					shapes << Shape{ rect.rotatedAt(center,angle).asPolygon(),color };
					break;
				}
				case 2:
				{
					const auto star = Shape2D::Star(50, center, angle);
					shapes << Shape{ star.asPolygon(),color };
					break;
				}
				case 3:
				{
					const auto plus = Shape2D::Plus(50, 20, center, angle);
					shapes << Shape{ plus.asPolygon(),color };
					break;
				}
				}
			}
			target_index = Random(shapes.size() - 1);
			grab_index = hold_index = none;
		}
		void update() override {
			if (timer.reachedZero()) {
				changeScene(
					SceneFuctory::Create<GameScene2>(),
					TransitionFuctory::Create<Yeah::Transitions::Step>()
				);
			}
		}
		void draw() const override {
			font(U"探せ！").drawAt({ 400,200 });
			const auto& target = shapes[target_index];
			target.polygon.movedBy(-target.polygon.centroid()).scaled(0.6 * (2 + Periodic::Sine0_1(2s))).movedBy(Vec2(400, 400)).draw(target.color);
		}
	};
	class GameScene2 :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		Optional<int32> mouseover_index;
	public:
		void update() override {
			mouseover_index = none;
			for (const auto& [i, s] : IndexedReversed(shapes)) {
				const auto& [polygon, color] = s;
				if (polygon.leftClicked()) {
					grab_index = i;
				}
				if (grab_index && MouseL.pressedDuration() > 0.1s) {
					hold_index = *grab_index;
				}
				if (grab_index && MouseL.up()) {
					if (not hold_index) {
						changeScene(
							SceneFuctory::Create<Result>(*grab_index == target_index),
							TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
						);
					}
					grab_index = hold_index = none;
				}
				if (polygon.mouseOver()) {
					mouseover_index = i;
					break;
				}
			}

			if (hold_index) {
				shapes[*hold_index].polygon.moveBy(Cursor::PosF() - shapes[*hold_index].polygon.centroid());
			}
		}
		void draw() const override {
			for (const auto& [polygon, color] : shapes) {
				polygon.draw(Vec2(3, 3), ColorF(Palette::Lightblue, 0.5));
				polygon.draw(color);
			}
			if (mouseover_index) {
				shapes[*mouseover_index].polygon.drawFrame(3, Palette::Yellow);
				Print << (*mouseover_index == target_index);
			}
		}
	};
	class Result :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		const bool success_ = false;
	public:
		Result() {}
		Result(bool success) :
			success_(success) {}
		void update() override {
			if (SimpleGUI::ButtonAt(U"答え", { 400,400 }, 200)) {
				changeScene(
					SceneFuctory::Create<Answer>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,450 }, 200)) {
				changeScene(
					SceneFuctory::Create<Title>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(success_ ? U"クリア！！！" : U"残念！").drawAt({ 400,180 });
		}
	};
	class Answer :public Yeah::Scenes::IScene {
		SaturatedLinework<Circle> sl_{ Circle(shapes[target_index].polygon.centroid(),50) };
		mutable std::function<void()> banana;
	public:
		void update() override {
			if (banana) {
				banana();
			}
		}
		void draw() const override {
			for (const auto& [polygon, color] : shapes) {
				polygon.draw(Vec2(3, 3), ColorF(Palette::Lightblue, 0.5));
				polygon.draw(color);
			}
			shapes[target_index].polygon.drawFrame(5, Palette::Yellow);
			sl_.draw();

			if (SimpleGUI::Button(U"戻る", { 0,0 }, 70)) {
				banana = [this]() mutable {
					const_cast<Answer*>(this)->back();
				};
			}
		}
	};
}
namespace CountFace {
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"イージー", { 400,350 }, 200)) {
				//changeScene(
				//	sceneFuctory.create(U"CountFaceRule"),
				//	std::make_unique<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				//);
			}
			if (SimpleGUI::ButtonAt(U"ノーマル", { 400,400 }, 200)) {
				//changeScene(
				//	sceneFuctory.create(U"FindShapeGameScene1"),
				//	std::make_unique<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				//);
			}
			if (SimpleGUI::ButtonAt(U"ハード", { 400,450 }, 200)) {
				//changeScene(
				//	sceneFuctory.create(U"FindShapeGameScene1"),
				//	std::make_unique<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				//);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFuctory::Create<Master::Title>(),
					TransitionFuctory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(U"うんち！").drawAt({ 400,180 });
		}
	};
	class Rule :public Yeah::Scenes::IScene {
		const Font font{ 100 }, font50{ 50 };

	public:
		void update() override {

		}
		void draw() const override {
			font(U"ルール").drawAt({ 400,120 });
			font50(U"顔が右から左に流れていく\n指定された顔の数を数えよう").drawAt({ 400,320 });
		}
	};
}
namespace ConwaysGameOfLife {

}

template<typename T, typename...Args>
std::unique_ptr<Yeah::Scenes::IScene> SceneFuctory::Create(Args&&...args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

void Main() {
	Window::SetPos({ 1000,200 });
	Scene::SetBackground(ColorF(0.2, 0.3, 0.4));
	Yeah::SceneChanger sc;
	sc.change(SceneFuctory::Create<Master::Title>());
	while (System::Update()) {
		ClearPrint();
		if (not sc.update()) {
			break;
		}
		sc.draw();
	}
}
