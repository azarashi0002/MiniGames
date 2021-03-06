
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

			//if (KeyU.down()) {
			//	undo(TransitionFactory::Create<Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			//}
			//if (KeyR.down()) {
			//	redo(TransitionFactory::Create<Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			//}

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
namespace Second {
	class Title;
}
namespace ConwaysGameOfLife {
	class Title;
	class Game;
}
namespace BreakOut {
	class Title;
	class Game;
	class Game2;
	class Result;
}
namespace FindShape {
	class Title;
	class GameScene1;
	class GameScene2;
	class Result;
	class Answer;
}
namespace TenSecondsTimer {
	class Title;
	class Rule;
	class Game;
	class Result;
}

/*シーン実装*/
namespace Master {
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"ライフゲーム", { 400,350 }, 200)) {
				changeScene(
					SceneFactory::Create<ConwaysGameOfLife::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ブロック崩し", { 400,400 }, 200)) {
				changeScene(
					SceneFactory::Create<BreakOut::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"供養ゲーム", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Second::Title>(),
					TransitionFactory::Create<Yeah::Transitions::CustomFadeInOut<Yeah::Transitions::AlphaFadeOut, Yeah::Transitions::AlphaFadeIn>>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"終了", { 400,500 }, 200)) {
				exit();
			}
		}
		void draw() const override {
			font(U"MiniGames").drawAt({ 400,180 });
		}
	};
}
namespace Second {
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
			if (SimpleGUI::ButtonAt(U"10秒タイマー", { 400,400 }, 200, true)) {
				changeScene(
					SceneFactory::Create<TenSecondsTimer::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"", { 400,450 }, 200, false)) {
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Master::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(U"MiniGames").drawAt({ 400,180 });
		}
	};
}
namespace ConwaysGameOfLife {
	class Impl {	//ライフゲーム本体
	public:
		Grid<bool> cell_;

		Impl(const Size& size, bool value = false) :
			cell_(size, value) {}
		void update() {
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
		void draw() const {
			for (const auto& p : step(cell_.size())) {
				RectF(p, 1).draw(cell_[p] ? Palette::Yellow : Palette::Gray).drawFrame(0.05, 0.0, Palette::Black);
			}
		}
	};

	class Title :public Yeah::Scenes::IScene {
		Impl impl_{ Size(40,30) };
		Timer timer{ 2s,true };
		const Font font{ 100 };
	public:
		Title() {
			for (auto&& i : impl_.cell_) {
				i = RandomBool(0.3);
			}
		}
		void update() override {
			impl_.update();
			if (timer.reachedZero()) {
				for (auto&& i : impl_.cell_) {
					i = RandomBool(0.3);
				}
				timer.restart();
			}

			if (SimpleGUI::ButtonAt(U"スタート", { 400,400 }, 200)) {
				changeScene(
					SceneFactory::Create<Game>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Master::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			{
				const Transformer2D t(Mat3x2::Translate(-10, -7.5).scaled(40));
				const ScopedColorMul2D s(1.0, 0.1);
				impl_.draw();
			}
			font(U"ライフゲーム").drawAt({ 400,180 });
		}
	};
	class Game :public Yeah::Scenes::IScene {
		Impl impl_{ Size(30,30) };
		bool auto_ = false;
	public:
		void update() override {
			{
				const Transformer2D t(Mat3x2::Scale(20), true);
				for (const auto& p : step(impl_.cell_.size())) {
					if (const auto& region = RectF(p, 1); region.leftPressed()) {
						impl_.cell_[p] = true;
					}
					else if (region.rightPressed()) {
						impl_.cell_[p] = false;
					}
				}
			}

			if (SimpleGUI::ButtonAt(U"次へ", { 700,50 }, 160, not auto_)) {
				impl_.update();
			}
			SimpleGUI::CheckBoxAt(auto_, U"オート", { 700,100 }, 160);
			if (auto_) {
				impl_.update();
			}
			if (SimpleGUI::ButtonAt(U"ランダム", { 700,200 }, 160)) {
				const double chance = Random(0.1, 0.5);
				for (auto&& i : impl_.cell_) {
					i = RandomBool(chance);
				}
			}
			if (SimpleGUI::ButtonAt(U"リセット", { 700,250 }, 160)) {
				impl_.cell_.fill(false);
			}

			if (SimpleGUI::ButtonAt(U"戻る", { 700,550 }, 160) || KeyB.down()) {
				undo(TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}
		}
		void draw() const override {
			const Transformer2D t(Mat3x2::Scale(20), true);
			impl_.draw();
		}
	};
}
namespace BreakOut {
	class Impl {	//ゲーム本体の実装
	public:
		struct Block {
			RectF region;
			ColorF color;
			int32 life;

			void draw() const {
				region.stretched(-1).draw(color);
			}
		};
		Size block_size_{ 40,25 };
		Size blocks_num_{ 16,7 };
		static constexpr Point blocks_center{ 400,150 };
		double ball_speed = 400.0;
		Array<Block> blocks_;
		Vec2 ball_vel_ = Vec2::Zero();
		Circle ball_{ 0,0,8 };
		RectF paddle_{ 0,0,60,10 };
		int32 score_ = 0;
		Stopwatch sw;

		bool hold = true;

		Impl(const Size& block_size, const Size& blocks_num):
			block_size_(block_size),
			blocks_num_(blocks_num) {
			for (const auto& i : step(blocks_num_)) {
				blocks_ << Block{ RectF(Arg::center = blocks_center - (i - (blocks_num_ - Vec2::One()) / 2.0) * block_size_,block_size_),RandomColorF(),1 };
			}
		}

		bool update() {
			paddle_.setPos(Arg::center = Vec2{ Cursor::PosF().x, 500 });

			if (hold && MouseL.down()) {
				hold = false;
				ball_vel_ = Vec2::Up(ball_speed);
				sw.start();
			}

			if (hold) {
				ball_.setPos(Arg::bottomCenter = paddle_.topCenter());
				ball_vel_ = Vec2::Zero();
			}
			else {
				ball_.moveBy(ball_vel_ * Scene::DeltaTime());
			}

			for (auto i = begin(blocks_); i != end(blocks_); ++i) {
				if (i->region.intersects(ball_)) {
					if (i->region.right().intersects(ball_) || i->region.left().intersects(ball_)) {
						ball_vel_.x *= -1;
					}
					if (i->region.top().intersects(ball_) || i->region.bottom().intersects(ball_)) {
						ball_vel_.y *= -1;
					}

					if (--(i->life) <= 0) {
						blocks_.erase(i);
						++score_;
						ball_speed += 5;
					}
					break;
				}
			}

			if ((ball_.x < 0 && ball_vel_.x < 0) || (800 < ball_.x && ball_vel_.x > 0)) {
				ball_vel_.x *= -1;
			}
			if (ball_.y < 0 && ball_vel_.y < 0) {
				ball_vel_.y *= -1;
			}
			if (ball_vel_.y > 0 && paddle_.intersects(ball_)) {
				ball_vel_ = Vec2((ball_.x - paddle_.center().x) * 10, -ball_vel_.y).setLength(ball_speed);
			}

			ball_vel_.setLength(ball_speed);

			if (ball_.y > 600) {
				return false;
			}
			if (blocks_.empty()) {
				return false;
			}

			return true;
		}
		void draw() const {
			for (const auto& i : blocks_) {
				i.draw();
			}
			ball_.draw();
			paddle_.draw();
		}
	};

	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		Impl impl_{ {40,25},{16,7} };
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"スタート", { 400,350 }, 200)) {
				changeScene(
					SceneFactory::Create<Game>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			{
				const ScopedColorMul2D s(1.0, 0.0);
				if (SimpleGUI::ButtonAt(U"ハード", { 400,400 }, 200)) {
					changeScene(
						SceneFactory::Create<Game2>(),
						TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
					);
				}
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Master::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			{
				const Transformer2D t(Mat3x2::Translate(0, 150).scaled(3, {400,150}));
				const ScopedColorMul2D s(1.0, 0.1);
				impl_.draw();
			}
			font(U"ブロック崩し").drawAt({ 400,180 }, Palette::White);
		}
	};
	class Game :public Yeah::Scenes::IScene {
		Impl impl_{ {40,25},{16,7} };
	public:
		void update() override {
			if (not impl_.update()) {
				changeScene(
					SceneFactory::Create<Result>(impl_.score_, impl_.sw.elapsed(), SceneFactory::Create<Game>),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			impl_.draw();
		}
	};
	class Game2 :public Yeah::Scenes::IScene {
		Impl impl_{ {20,10},{35,20} };
	public:
		void update() override {
			if (not impl_.update()) {
				changeScene(
					SceneFactory::Create<Result>(impl_.score_, impl_.sw.elapsed(), SceneFactory::Create<Game2>),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			impl_.draw();
		}
	};

	class Result :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		int32 score_;
		Duration duration_;
		std::unique_ptr<Yeah::Scenes::IScene>(*factory_)();
	public:
		Result(int32 score, const Duration& duration, std::unique_ptr<Yeah::Scenes::IScene>(*factory)()) :
			score_(score),
			duration_(duration),
			factory_(factory) {}
		void update() override {
			if (SimpleGUI::ButtonAt(U"もう一度", { 400,450 }, 200)) {
				changeScene(
					factory_(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(U"スコア:{}"_fmt(score_)).drawAt({ 400,180 });
			font(U"タイム:{:.2f}s"_fmt(duration_.count())).drawAt({ 400,300 });
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
					SceneFactory::Create<Second::Title>(),
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
namespace TenSecondsTimer {
	class Title :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		const std::array<Texture, 12> clocks = {
			Texture{Emoji(U"🕛")},
			Texture{Emoji(U"🕐")},
			Texture{Emoji(U"🕑")},
			Texture{Emoji(U"🕒")},
			Texture{Emoji(U"🕓")},
			Texture{Emoji(U"🕔")},
			Texture{Emoji(U"🕕")},
			Texture{Emoji(U"🕖")},
			Texture{Emoji(U"🕗")},
			Texture{Emoji(U"🕘")},
			Texture{Emoji(U"🕙")},
			Texture{Emoji(U"🕚")},
		};
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"スタート", { 400,350 }, 200)) {
				changeScene(
					SceneFactory::Create<Game>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"ルール", { 400,400 }, 200)) {
				changeScene(
					SceneFactory::Create<Rule>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Second::Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			clocks[static_cast<int32>(Scene::Time()) % clocks.size()].scaled(3).drawAt({ 400,300 }, ColorF(1.0, 0.1));
			font(U"10秒タイマー").drawAt({ 400,180 });
		}
	};
	class Rule :public Yeah::Scenes::IScene {
		const Font font{ 100 }, font50{ 50 };
		const std::array<Texture, 12> clocks = {
			Texture{Emoji(U"🕛")},
			Texture{Emoji(U"🕐")},
			Texture{Emoji(U"🕑")},
			Texture{Emoji(U"🕒")},
			Texture{Emoji(U"🕓")},
			Texture{Emoji(U"🕔")},
			Texture{Emoji(U"🕕")},
			Texture{Emoji(U"🕖")},
			Texture{Emoji(U"🕗")},
			Texture{Emoji(U"🕘")},
			Texture{Emoji(U"🕙")},
			Texture{Emoji(U"🕚")},
		};
	public:
		void update() override {
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				undo(TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s));
			}
		}
		void draw() const override {
			clocks[static_cast<int32>(Scene::Time()) % clocks.size()].scaled(3).drawAt({ 400,300 }, ColorF(1.0, 0.1));
			font(U"ルール").drawAt({ 400,120 });
			font50(U"カウントダウン後にタイマーが\nスタートする\n10秒経ったらボタンを押そう").drawAt({ 400,320 });
		}
	};
	class Game :public Yeah::Scenes::IScene {
		const Font font{ 100 };
		const std::array<Texture, 12> clocks = {
			Texture{Emoji(U"🕛")},
			Texture{Emoji(U"🕐")},
			Texture{Emoji(U"🕑")},
			Texture{Emoji(U"🕒")},
			Texture{Emoji(U"🕓")},
			Texture{Emoji(U"🕔")},
			Texture{Emoji(U"🕕")},
			Texture{Emoji(U"🕖")},
			Texture{Emoji(U"🕗")},
			Texture{Emoji(U"🕘")},
			Texture{Emoji(U"🕙")},
			Texture{Emoji(U"🕚")},
		};
		enum class State {
			Wait,
			CountDown,
			Time,
			Finish,
			Size,
		} state = State::Wait;
		Timer count_down_{ 3s };
		Stopwatch result_;
	public:
		void update() override {
			switch (state) {
			case State::Wait:
				if (SimpleGUI::ButtonAt(U"準備OK！", { 400,300 }, 200)) {
					state = State::CountDown;
					count_down_.start();
				}
				break;
			case State::CountDown:
				if (count_down_.reachedZero()) {
					state = State::Time;
					result_.start();
				}
				break;
			case State::Time:
			case State::Finish:
				if (SimpleGUI::ButtonAt(U"ストップ！", { 400,300 }, 200)) {
					state = State::Finish;
					changeScene(
						SceneFactory::Create<Result>(result_.elapsed()),
						TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
					);
				}
				break;
			}
		}
		void draw() const override {
			switch (state) {
			case State::Wait:
				clocks[static_cast<int32>(Scene::Time()) % clocks.size()].scaled(3).drawAt({ 400,300 }, ColorF(1.0, 0.1));
				break;
			case State::CountDown:
				font(Ceil(count_down_.remaining().count())).drawAt({ 400,300 });
				break;
			}
		}
	};
	class Result :public Yeah::Scenes::IScene {
		const Font font{ 100 }, font50{ 50 };
		Duration duration_;
	public:
		Result(const Duration& duration) :
			duration_(duration) {}
		void update() override {
			if (SimpleGUI::ButtonAt(U"もう一度", { 400,450 }, 200)) {
				changeScene(
					SceneFactory::Create<Game>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
			if (SimpleGUI::ButtonAt(U"戻る", { 400,500 }, 200)) {
				changeScene(
					SceneFactory::Create<Title>(),
					TransitionFactory::Create<Yeah::Transitions::AlphaFadeInOut>(0.4s, 0.4s)
				);
			}
		}
		void draw() const override {
			font(duration_).drawAt({ 400,250 }, Palette::White);
			font50(Abs((duration_ - 10s).count()) <= 0.5 ? U"お見事！" : U"もう一度！").drawAt({ 400,350 }, Palette::White);
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
	Profiler::EnableAssetCreationWarning(false);

	Window::SetTitle(U"MiniGames");
	Window::SetPos({ 1000,200 });
	Scene::SetBackground(ColorF(0.2, 0.3, 0.4));

	Yeah::SceneChanger sc(
		SceneFactory::Create<Master::Title>(),
		TransitionFactory::Create<Yeah::Transitions::AlphaFadeIn>(1s)
	);
	while (System::Update()) {
		if (not sc.update()) {
			break;
		}
		sc.draw();
	}
}
