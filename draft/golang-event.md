
# golang-event



##     ethereum  event  库的使用

 github.com/ethereum/go-ethereum/event 包 实现了一个事件发布订阅的库,
 使用接口主要是event.Feed 类型， 以前还有event.TypeMux 类型，看代码注释，说过时， 目前主要使用Feed 类型.
 
```golang
package main

import (
	"fmt"
    "sync"
    "github.com/ethereum/go-ethereum/event"
)

func main() {
	type someEvent struct{ I int }

	var feed event.Feed
	var wg sync.WaitGroup

	ch := make(chan someEvent)
	sub := feed.Subscribe(ch)

	wg.Add(1)
	go func() {
		defer wg.Done()
		for event := range ch {
			fmt.Printf("Received: %#v\n", event.I)
		}
		sub.Unsubscribe()
		fmt.Println("done")
	}()

	feed.Send(someEvent{5})
	feed.Send(someEvent{10})
	feed.Send(someEvent{7})
	feed.Send(someEvent{14})
	close(ch)

	wg.Wait()
}
```
通过调用event.Feed 类型的Subscrible方法订阅事件通知， 需要使用者提前指定接收事件的channel， Subscribe返回Subscription 对象， 是一个接口类型:
```golang
type Subscription interface {
            Err() <-chan error      // returns the error channel
            Unsubscribe()           // cancels sending of events, closing the error channel
}
```
Err() 返回获取error 的channel， 调用Unsubscribe()取消事件订阅。
事件的发布者调用 Send() 方法， 发送事件。
可以使用同一个channel实例，多次调用Feed 的Subscrible 方法:
```golang
package main

import (
	"fmt"
	"sync"

	"github.com/ethereum/go-ethereum/event"
)

func main() {

	var (
		feed   event.Feed
		recv   sync.WaitGroup
		sender sync.WaitGroup
	)

	ch := make(chan int)
	feed.Subscribe(ch)
	feed.Subscribe(ch)
	feed.Subscribe(ch)

	expectSends := func(value, n int) {
		defer sender.Done()
		if nsent := feed.Send(value); nsent != n {
			fmt.Printf("send delivered %d times, want %d\n", nsent, n)
		}
	}
	expectRecv := func(wantValue, n int) {
		defer recv.Done()
		for v := range ch {
			if v != wantValue {
				fmt.Printf("received %d, want %d\n", v, wantValue)
			} else {
				fmt.Printf("recv v = %d\n", v)
			}
		}
	}

	sender.Add(3)
	for i := 0; i < 3; i++ {
		go expectSends(1, 3)
	}
	go func() {
		sender.Wait()
		close(ch)
	}()
	recv.Add(1)
	go expectRecv(1, 3)
	recv.Wait()
}
```
这个例子中， 有三个订阅者， 有三个发送者， 每个发送者发送三次1， 同一个channel ch 里面被推送了九个1.

ethereum event 库还提供了一些高级别的方便接口， 比如event.NewSubscription函数，接收一个函数类型，作为数据的生产者， producer本身在后台一个单独的goroutine内执行， 后台goroutine往用户的channel 发送数据:
```golang
package main

import (
	"fmt"

	"github.com/ethereum/go-ethereum/event"
)

func main() {
	ch := make(chan int)
	sub := event.NewSubscription(func(quit <-chan struct{}) error {
		for i := 0; i < 10; i++ {
			select {
			case ch <- i:
			case <-quit:
				fmt.Println("unsubscribed")
				return nil
			}
		}
		return nil
	})

	for i := range ch {
		fmt.Println(i)
		if i == 4 {
			sub.Unsubscribe()
			break
		}
	}
}
```
库也提供了event.SubscriptionScope类型用于追踪多个订阅者，提供集中的取消订阅功能:
```golang
package main

import (
	"fmt"
	"sync"

	"github.com/ethereum/go-ethereum/event"
)

// This example demonstrates how SubscriptionScope can be used to control the lifetime of
// subscriptions.
//
// Our example program consists of two servers, each of which performs a calculation when
// requested. The servers also allow subscribing to results of all computations.
type divServer struct{ results event.Feed }
type mulServer struct{ results event.Feed }

func (s *divServer) do(a, b int) int {
	r := a / b
	s.results.Send(r)
	return r
}

func (s *mulServer) do(a, b int) int {
	r := a * b
	s.results.Send(r)
	return r
}

// The servers are contained in an App. The app controls the servers and exposes them
// through its API.
type App struct {
	divServer
	mulServer
	scope event.SubscriptionScope
}

func (s *App) Calc(op byte, a, b int) int {
	switch op {
	case '/':
		return s.divServer.do(a, b)
	case '*':
		return s.mulServer.do(a, b)
	default:
		panic("invalid op")
	}
}

// The app's SubscribeResults method starts sending calculation results to the given
// channel. Subscriptions created through this method are tied to the lifetime of the App
// because they are registered in the scope.
func (s *App) SubscribeResults(op byte, ch chan<- int) event.Subscription {
	switch op {
	case '/':
		return s.scope.Track(s.divServer.results.Subscribe(ch))
	case '*':
		return s.scope.Track(s.mulServer.results.Subscribe(ch))
	default:
		panic("invalid op")
	}
}

// Stop stops the App, closing all subscriptions created through SubscribeResults.
func (s *App) Stop() {
	s.scope.Close()
}

func main() {
	var (
		app  App
		wg   sync.WaitGroup
		divs = make(chan int)
		muls = make(chan int)
	)

	divsub := app.SubscribeResults('/', divs)
	mulsub := app.SubscribeResults('*', muls)
	wg.Add(1)
	go func() {
		defer wg.Done()
		defer fmt.Println("subscriber exited")
		for {
			select {
			case result := <-divs:
				fmt.Println("division happened:", result)
			case result := <-muls:
				fmt.Println("multiplication happened:", result)
			case divErr := <-divsub.Err():
				fmt.Println("divsub.Err() :", divErr)
				return
			case mulErr := <-mulsub.Err():
				fmt.Println("mulsub.Err() :", mulErr)
				return
			}
		}
	}()

	app.Calc('/', 22, 11)
	app.Calc('*', 3, 4)

	app.Stop()
	wg.Wait()
}
```
SubscriptionScope的Close() 方法接收Track方法的返回值 ， Track 方法负责追踪订阅者。

