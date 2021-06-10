
#include<Siv3D.hpp>
#include<HamFramework.hpp>

namespace Yeah {
	namespace Scenes { class IScene; }
	namespace Transitions { class ITransition; }
	class SceneChanger;
}

class SceneFactory {
public:
	template<typename T, typename...Args>
	static std::unique_ptr<Yeah::Scenes::IScene> Create(Args&&...args);
};
namespace Yeah {
	namespace Scenes {
		class IScene {
			friend class SceneChanger;

			struct {
				bool exit_ = false;
				Optional<std::pair<std::unique_ptr<IScene>, std::unique_ptr<Transitions::ITransition>>> change_;
				Optional<std::unique_ptr<Transitions::ITransition>> undo_;
				Optional<std::unique_ptr<Transitions::ITransition>> redo_;

				void resetOptional() {
					change_.reset();
					undo_.reset();
					redo_.reset();
				}
			} request_;
		public:
			virtual ~IScene() {}
			virtual void initialize() {}	//シーンが呼ばれたとき(undo・redoでも呼ばれる)

			virtual void update() = 0;
			virtual void draw() const = 0;

			virtual void updateFadeIn(double /*t*/) { update(); }
			virtual void updateFadeOut(double /*t*/) { update(); }
			virtual void drawFadeIn(double /*t*/) const { draw(); }
			virtual void drawFadeOut(double /*t*/) const { draw(); }

			void exit() {
				request_.exit_ = true;
			}
			void changeScene(std::unique_ptr<IScene>&& scene,
				std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
				request_.change_ = std::make_pair(std::move(scene), std::move(transition));
			}
			void undo(std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
				request_.undo_ = std::move(transition);
			}
			void redo(std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
				request_.redo_ = std::move(transition);
			}
		};
	}
}

class TransitionFactory {
public:
	template<typename T, typename...Args>
	static std::unique_ptr<Yeah::Transitions::ITransition> Create(Args&&...args);
};
namespace Yeah {
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
					return Optional<std::unique_ptr<ITransition>>(TransitionFactory::Create<Transitions::Step>());
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
					return Optional<std::unique_ptr<ITransition>>(TransitionFactory::Create<Transitions::Step>());
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
					return Optional<std::unique_ptr<ITransition>>(TransitionFactory::Create<Transitions::Step>());
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
					return Optional<std::unique_ptr<ITransition>>(TransitionFactory::Create<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};

		template<typename FadeOutT, typename FadeInT>
		class CustomFadeInOut :public ITransition {
			Timer timer;
			const Duration fadeOutTime, fadeInTime;
			Optional<FadeOutT> fadeOut;
			Optional<FadeInT> fadeIn;
		public:
			CustomFadeInOut(const Duration& fadeOutTime, const Duration& fadeInTime) :
				timer(fadeOutTime + fadeInTime, true),
				fadeInTime(fadeInTime),
				fadeOutTime(fadeOutTime),
				fadeOut(FadeOutT{ fadeOutTime }) {}
			void update(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) override {
				if (not fadeIn && timer.remaining() < fadeInTime) {
					fadeIn.emplace(fadeInTime);
				}
				if (timer.remaining() < fadeInTime) {
					if (fadeIn) {
						fadeIn->update(before, after);
					}
				}
				else {
					if (fadeOut) {
						fadeOut->update(before, after);
					}
				}
			}
			void draw(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const override {
				if (timer.remaining() < fadeInTime) {
					if (fadeIn) {
						fadeIn->draw(before, after);
					}
				}
				else {
					if (fadeOut) {
						fadeOut->draw(before, after);
					}
				}
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				if (timer.reachedZero()) {
					return Optional<std::unique_ptr<ITransition>>(TransitionFactory::Create<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};

		template<typename FadeOutT, typename FadeInT>
		class CustomCrossFade :public ITransition {
			Timer timer;
			FadeOutT fadeOut;
			FadeInT fadeIn;
		public:
			CustomCrossFade(const Duration& fadeTime) :
				timer(fadeTime, true),
				fadeOut(FadeOutT{ fadeTime }),
				fadeIn(FadeInT{ fadeTime }) {}
			void update(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) override {
				fadeIn.update(before, after);
				fadeOut.update(before, after);
			}
			void draw(const std::unique_ptr<Scenes::IScene>& before,
				const std::unique_ptr<Scenes::IScene>& after) const override {
				fadeIn.draw(before, after);
				fadeOut.draw(before, after);
			}

			Optional<std::unique_ptr<ITransition>> nextTransition() const override {
				if (timer.reachedZero()) {
					return Optional<std::unique_ptr<ITransition>>(TransitionFactory::Create<Transitions::Step>());
				}
				else {
					return none;
				}
			}
		};
	}
}

namespace Yeah {
	class SceneChanger {
		Array<std::unique_ptr<Scenes::IScene>> scenes_;
		Optional<int64> before_index_, after_index_;
		std::unique_ptr<Transitions::ITransition> transition_ = TransitionFactory::Create<Transitions::CrossFade>(1s);
	public:
		SceneChanger() = default;
		SceneChanger(
			std::unique_ptr<Scenes::IScene>&& scene,
			std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
			change(std::move(scene), std::move(transition));
		}

		void setTransition(std::unique_ptr<Transitions::ITransition>&& transition) {
			if (not transition) { return; }

			transition_ = std::move(transition);
		}
		void change(std::unique_ptr<Scenes::IScene>&& next, std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
			if (not next) { return; }

			if (after_index_) {
				scenes_.dropBack(scenes_.size() - 1 - *after_index_);
			}

			scenes_ << std::move(next);

			before_index_ = after_index_;
			if (after_index_) {
				++(*after_index_);
			}
			else {
				after_index_ = 0;
			}
			
			if (after()) {
				after()->initialize();
			}

			setTransition(std::move(transition));
		}
		void redo(std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
			if (not after_index_) { return; }
			if (scenes_.size() <= *after_index_ + 1) { return; }

			before_index_ = after_index_;
			++(*after_index_);

			if (after()) {
				after()->initialize();
			}

			setTransition(std::move(transition));
		}
		void undo(std::unique_ptr<Transitions::ITransition>&& transition = nullptr) {
			if (not after_index_) { return; }
			if (*after_index_ <= 0) { return; }

			before_index_ = after_index_;
			--(*after_index_);

			if (after()) {
				after()->initialize();
			}

			setTransition(std::move(transition));
		}

		bool update() {
			if (transition_) {
				transition_->update(before(), after());
			}

			if (after()) {
				if (after()->request_.change_) {
					change(
						std::move(after()->request_.change_->first),
						std::move(after()->request_.change_->second)
					);
				}
				if (after()->request_.undo_) {
					undo(std::move(*after()->request_.undo_));
				}
				if (after()->request_.redo_) {
					redo(std::move(*after()->request_.redo_));
				}
			}
			if (after()) {
				after()->request_.resetOptional();
			}
			if (before()) {
				before()->request_.resetOptional();
			}

			if (KeyU.down()) {
				undo(TransitionFactory::Create<Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}
			if (KeyR.down()) {
				redo(TransitionFactory::Create<Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}

			if (transition_) {
				if (auto&& i = transition_->nextTransition(); i.has_value()) {
					setTransition(std::move(*i));
				}
			}

			return after() ? not after()->request_.exit_ : true;
		}
		void draw() const {
			if (transition_) {
				transition_->draw(before(), after());
			}

			Print << U"Transition:" << (transition_ ? Unicode::Widen(typeid(*transition_).name()) : U"Null");
			Print << U"Before:" << (before() ? Unicode::Widen(typeid(*before()).name()) : U"Null");
			Print << U"After:" << (after() ? Unicode::Widen(typeid(*after()).name()) : U"Null");
			Print << U"SceneNum:" << scenes_.size();
			Print << U"BeforeIndex:" << (before_index_ ? U"{}"_fmt(*before_index_) : U"Null");
			Print << U"AfterIndex:" << (after_index_ ? U"{}"_fmt(*after_index_) : U"Null");
			if (before()) {
				if (before()->request_.change_) {
					Print << U"Before::Change::Scene:" << (before()->request_.change_->first ? Unicode::Widen(typeid(*before()->request_.change_->first).name()) : U"Null");
					Print << U"Before::Change::Transition:" << (before()->request_.change_->second ? Unicode::Widen(typeid(*before()->request_.change_->second).name()) : U"Null");
				}
				else {
					Print << U"Before::Change:" << U"Null";
				}
				Print << U"Before::Undo:" << (before()->request_.undo_ ? Unicode::Widen(typeid(*before()->request_.undo_).name()) : U"Null");
				Print << U"Before::Redo:" << (before()->request_.redo_ ? Unicode::Widen(typeid(*before()->request_.redo_).name()) : U"Null");
			}
		}

	private:
		std::unique_ptr<Yeah::Scenes::IScene>& before() {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return before_index_ ? scenes_[*before_index_] : nul;
		}
		const std::unique_ptr<Yeah::Scenes::IScene>& before() const {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return before_index_ ? scenes_[*before_index_] : nul;
		}
		std::unique_ptr<Yeah::Scenes::IScene>& after() {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return after_index_ ? scenes_[*after_index_] : nul;
		}
		const std::unique_ptr<Yeah::Scenes::IScene>& after() const {
			static std::unique_ptr<Yeah::Scenes::IScene> nul{ nullptr };
			return after_index_ ? scenes_[*after_index_] : nul;
		}
	};
}

/*シーンの前方宣言*/
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
namespace ConwaysGameOfLife {
	class Title;
	class Rule;
	class Game;
}

/*シーン実装*/
namespace Master {
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"図形探し", { 400,350 }, 200)) {
				changeScene(
					SceneFactory::Create<FindShape::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"クソゲー2", { 400,400 }, 200)) {
				changeScene(
					SceneFactory::Create<CountFace::Title>(),
					TransitionFactory::Create<Yeah::Transitions::CustomFadeInOut<Yeah::Transitions::AlphaFadeOut, Yeah::Transitions::AlphaFadeIn>>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ライフゲーム", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<ConwaysGameOfLife::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"終了", { 400,500 }, 200)) {
				exit();
			}
		}
		void draw() const override {
			font(U"Title!!!!!!").drawAt({ 400,180 });
		}
	};
}
namespace FindShape {
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"イージー", { 400,350 }, 200)) {
				changeScene(
					SceneFactory::Create<GameScene1>(50),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ノーマル", { 400,400 }, 200)) {
				changeScene(
					SceneFactory::Create<GameScene1>(100),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ハード", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<GameScene1>(200),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Master::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
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
	Optional<int32> hold_index;	//0.15秒以上ホールド
	class GameScene1 :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		Timer timer{ 3s,true };
		int32 shapenum_;
	public:
		GameScene1(int32 shapenum) :
			shapenum_(shapenum) {}
		void initialize() override {
			timer.restart();

			shapes.clear();
			for ([[maybe_unused]] int i : step(shapenum_)) {
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
					SceneFactory::Create<GameScene2>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.1s, 0.1s)
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
				if (grab_index && MouseL.pressedDuration() > 0.15s) {
					hold_index = *grab_index;
				}
				if (grab_index && MouseL.up()) {
					if (not hold_index) {
						changeScene(
							SceneFactory::Create<Result>(*grab_index == target_index),
							TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
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
				//Print << (*mouseover_index == target_index);
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
					SceneFactory::Create<Answer>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(success_ ? U"クリア！！！" : U"残念！").drawAt({ 400,180 });
		}
	};
	class Answer :public Yeah::Scenes::IScene {
		SaturatedLinework<Circle> linework_{ Circle(shapes[target_index].polygon.centroid(),100) };
		mutable std::function<void()> delay;
	public:
		void update() override {
			if (delay) {
				delay();
				delay = decltype(delay)();
			}

			linework_.generate();
		}
		void draw() const override {
			for (const auto& [polygon, color] : shapes) {
				polygon.draw(Vec2(3, 3), ColorF(Palette::Lightblue, 0.5));
				polygon.draw(color);
			}
			shapes[target_index].polygon.drawFrame(5, Palette::Yellow);
			linework_.draw();

			if (SimpleGUI::Button(U"戻る", { 0,0 }, 70)) {
				delay = [this]() mutable {
					const_cast<Answer*>(this)->undo(TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s));
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
			}
			if (SimpleGUI::ButtonAt(U"ノーマル", { 400,400 }, 200)) {
			}
			if (SimpleGUI::ButtonAt(U"ハード", { 400,450 }, 200)) {
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Master::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
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
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"スタート", { 400,400 }, 200)) {
				changeScene(
					SceneFactory::Create<Game>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ルール", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Rule>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Master::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(U"ライフゲーム").drawAt({ 400,180 });
		}
	};
	class Rule :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(U"ルールだよ！").drawAt({ 400,180 });
		}
	};
	class Game :public Yeah::Scenes::IScene {
		Grid<bool> cell_{ Size(30,30),false };
		bool auto_ = false;
	public:
		void update() override {
			for (const auto& p : step(cell_.size())) {
				if (const auto& region = Region(p); region.leftPressed()) {
					cell_[p] = true;
				}
				else if (region.rightPressed()) {
					cell_[p] = false;
				}
			}

			if (SimpleGUI::ButtonAt(U"次へ(N)", { 700,50 }, 160, not auto_) || ((not auto_) && KeyN.down())) {
				next();
			}
			SimpleGUI::CheckBoxAt(auto_, U"オート(A)", { 700,100 }, 160);
			auto_ ^= KeyA.down();
			if (auto_) {
				next();
			}
			if (SimpleGUI::ButtonAt(U"戻る(B)", { 700,550 }, 160) || KeyB.down()) {
				undo(TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}
		}
		void draw() const override {
			for (const auto& p : step(cell_.size())) {
				Region(p).draw(cell_[p] ? Palette::Yellow : Palette::Gray).drawFrame(1.0, Palette::Black);
			}
		}

	private:
		static RectF Region(const Point& p) {
			return { p * 20,20 };
		}

		void next() {
			Grid<int32> countAround(cell_.size(), 0);
			for (const auto& p : step(cell_.size())) {
				for (const auto& i : step(Point(-1, -1), Size(3, 3))) {
					if (i.isZero()) {
						continue;
					}
					countAround[p] += cell_.fetch(p + i, false);
				}
			}
			Grid<bool> tmp(cell_.size());
			for (const auto& p : step(cell_.size())) {
				tmp[p] = (countAround[p] == 3) || (countAround[p] == 2 && cell_[p]);
			}
			cell_ = std::move(tmp);
		}
	};
}

/*各ファクトリの定義*/
template<typename T, typename...Args>
std::unique_ptr<Yeah::Scenes::IScene> SceneFactory::Create(Args&&...args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}
template<typename T, typename...Args>
std::unique_ptr<Yeah::Transitions::ITransition> TransitionFactory::Create(Args&&...args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

void Main() {
	Window::SetPos({ 1000,200 });
	Scene::SetBackground(ColorF(0.2, 0.3, 0.4));

	Yeah::SceneChanger sc(
		SceneFactory::Create<Master::Title>(),
		TransitionFactory::Create<Yeah::Transitions::AlphaFadeIn>(1s)
	);
	while (System::Update()) {
		ClearPrint();
		if (not sc.update()) {
			break;
		}
		sc.draw();
	}
}
