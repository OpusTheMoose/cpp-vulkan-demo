module;

#include <memory>
export module observer;



export class Observer {
    public:
        virtual void update() = 0;
        virtual ~Observer() = default;
};

export class Subject {
    public:
        virtual void attach(std::weak_ptr<Observer>) = 0;
        virtual void detach(std::weak_ptr<Observer>) = 0;
        virtual void notify() = 0;
        virtual ~Subject() = default;

};