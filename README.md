# `madvise` の新規フラグについて

本記事は [Linux Advent Calendar](https://qiita.com/advent-calendar/2019/linux) の 12/9 分の記事です。

カーネル v5.4 で `madvise()` に新しいフラグとして `MADV_COLD` と `MADV_PAGEOUT` というフラグが追加されました。ちょうどよいタイミングなので、本記事ではこれらのフラグの動作と追加された背景などについて簡単に調べたものを共有します。

# `MADV_COLD`, `MADV_PAGEOUT`

`MADV_COLD` も `MADV_PAGEOUT` もページキャッシュや anonymous ページの回収処理を制御するフラグです。

`MADV_COLD` は指定したページは不要とマークしますが、積極的には回収のアクションは取らず、回収優先度の高い LRU リストに移して、回収はメモリ回収圧の有無に任せるという動作をします。別に即削除されても構わないけれど、他にメモリを積極的に割り当てようとするプロセスがなかった場合に役立つかもしれないので無理に回収せずキャッシュに置いておく、という考えのようです。

`MADV_PAGEOUT` はもう少し直接的な指示を与えるもので、指定した範囲のメモリをストレージに書き出した上で LRU リストからは即時削除される動作をします。ページキャッシュを破棄するという意味では昔からある `MADV_DONTNEED` と同様ですが、`MADV_DONTNEED` は dirty データをストレージに書き出さずに破棄するため、有用なデータがあるときに別途書き出しを実施する必要がありました。`MADV_PAGEOUT` はこの問題を解決しています。

## 簡単な実験

簡単な実験してみました。`mmap()` で割り当てた anonymous ページに対して madvise を実施した場合、前後で `/proc/PID/smaps` の当該メモリ領域がどのように変化したかをみます ([`sample2.c`](https://github.com/Naoya-Horiguchi/madvise_demo/blob/master/sample2.c))。

`MADV_COLD` の場合、下記のようになります。この出力は見やすさのために加工していて、通常 1 行に 1 エントリが出力されるところが、`madvise` 実行前、実行後、その後リードアクセスした後、の 3 時点の値を並べて表示しています。
~~~
700000000000-700000200000 rw-p 00000000 00:00 0
Size:               2048 kB     2048 kB 2048 kB
KernelPageSize:        4 kB     4 kB    4 kB
MMUPageSize:           4 kB     4 kB    4 kB
Rss:                   4 kB     4 kB    4 kB
Pss:                   4 kB     4 kB    4 kB
Shared_Clean:          0 kB     0 kB    0 kB
Shared_Dirty:          0 kB     0 kB    0 kB
Private_Clean:         0 kB     0 kB    0 kB
Private_Dirty:         4 kB     4 kB    4 kB
Referenced:            4 kB     0 kB    4 kB   // madvise(MADV_COLD) で unreferenced になる
Anonymous:             4 kB     4 kB    4 kB
LazyFree:              0 kB     0 kB    0 kB
AnonHugePages:         0 kB     0 kB    0 kB
ShmemPmdMapped:        0 kB     0 kB    0 kB
FilePmdMapped:        0 kB      0 kB    0 kB
Shared_Hugetlb:        0 kB     0 kB    0 kB
Private_Hugetlb:       0 kB     0 kB    0 kB
Swap:                  0 kB     0 kB    0 kB
SwapPss:               0 kB     0 kB    0 kB
Locked:                0 kB     0 kB    0 kB
~~~
`MADV_COLD` を実行しても anonymous ページ自体は残存していますが、referenced フラグがクリアされ、ページ回収における回収優先度が高くなります。

`MADV_PAGEOUT` も同様にチェックしてみると下記のようになります。
~~~
700000000000-700000200000 rw-p 00000000 00:00 0
Size:               2048 kB     2048 kB 2048 kB
KernelPageSize:        4 kB     4 kB    4 kB
MMUPageSize:           4 kB     4 kB    4 kB
Rss:                   4 kB     0 kB    4 kB   // ページが回収される
Pss:                   4 kB     0 kB    4 kB
Shared_Clean:          0 kB     0 kB    0 kB
Shared_Dirty:          0 kB     0 kB    0 kB
Private_Clean:         0 kB     0 kB    4 kB
Private_Dirty:         4 kB     0 kB    0 kB
Referenced:            4 kB     0 kB    4 kB
Anonymous:             4 kB     0 kB    4 kB
LazyFree:              0 kB     0 kB    0 kB
AnonHugePages:         0 kB     0 kB    0 kB
ShmemPmdMapped:        0 kB     0 kB    0 kB
FilePmdMapped:        0 kB      0 kB    0 kB
Shared_Hugetlb:        0 kB     0 kB    0 kB
Private_Hugetlb:       0 kB     0 kB    0 kB
Swap:                  0 kB     4 kB    0 kB   // swap out/in している。
SwapPss:               0 kB     4 kB    0 kB
Locked:                0 kB     0 kB    0 kB
~~~
`MADV_PAGEOUT` の場合 anonymous ページ自体は pageout されてストレージに書き出されます。anonymou ページのデータ自体は swap out されていることがわかる。余談になりますが、この動作は個人的には興味深く、これまで手軽に狙った anonymous ページを swap 領域に退避させる手段がなかった (cgroup で箱庭を作ってその中でメモリ圧を加えたりしていた) のが、`MADV_PAGEOUT` によって可能になるため、カーネル・ライブラリの関連機能のテストや検証時において非常に役立つのではないかと思います。

## ソースコード

ソースコード上 [`MADV_COLD`](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L485-L502) と [`MADV_PAGEOUT`](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L534-L554) は非常に似ていて、いずれも `cold_walk_ops` というコールバックを利用して page table walker を利用します。page table walker はページテーブルのツリーを指定したアドレス範囲内を走り、末端のエントリの状態に応じて詳細な処理を行うものです。今の場合、コールバック関数 `madvise_cold_or_pageout_pte_range` の[以下の箇所](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L439-L461)が最も重要な箇所です。

~~~
        for (; addr < end; pte++, addr += PAGE_SIZE) {
                ...
                /*
                 * We are deactivating a page for accelerating reclaiming.
                 * VM couldn't reclaim the page unless we clear PG_young.
                 * As a side effect, it makes confuse idle-page tracking
                 * because they will miss recent referenced history.
                 */
                ClearPageReferenced(page);
                test_and_clear_page_young(page);
                if (pageout) {
                        if (!isolate_lru_page(page)) {
                                if (PageUnevictable(page))
                                        putback_lru_page(page);
                                else
                                        list_add(&page->lru, &page_list);
                        }
                } else
                        deactivate_page(page);
        }
        ...
        if (pageout)
                reclaim_pages(&page_list);
~~~
共通の処理として対象ページを unreferenced にして回収優先度を上げ、`MADV_COLD` の場合は `deactiate_page()` で LRU リストを付け替えるだけですが、`MADV_PAGEOUT` の場合はいったんローカルリスト `page_list` によけて最後に一気に `reclaim_pages()` で回収していることがわかります。

今回この記事を書くきっかけになったフラグの説明は上記で済みましたが、他にもいくつかまだ十分知られていないフラグがあるので、せっかくなので見ていこうと思います。

# MADV_FREE

`MADV_FREE` フラグは v4.5 で導入されてから比較的時間が経っているので、[man](http://man7.org/linux/man-pages/man2/madvise.2.html) にも記載されています。その説明によると、`MADV_FREE` は不要なページを解放する指示を行うが、実際の解放はメモリ回収圧がかかったときという動作をします。ページ回収のタイミングがメモリ回収圧のときというのは `MADV_COLD` と同様ですが、回収時に dirty データが書き出されずに削除される点 (`MADV_DONTNEED` 的な動作) が `MADV_PAGEOUT` と異なります。

## 実験

`madvise(MADV_FREE)` の呼び出し前後の smaps の変化を見てみると下記のようになります。
~~~
700000000000-700000200000 rw-p 00000000 00:00 0         rw-p 00000000   rw-p 00000000
Size:               2048 kB     2048 kB 2048 kB
KernelPageSize:        4 kB     4 kB    4 kB
MMUPageSize:           4 kB     4 kB    4 kB
Rss:                   4 kB     4 kB    4 kB   // メモリは回収されず
Pss:                   4 kB     4 kB    4 kB
Shared_Clean:          0 kB     0 kB    0 kB
Shared_Dirty:          0 kB     0 kB    0 kB
Private_Clean:         0 kB     4 kB    4 kB   // dirty anonymous -> clean anonymous になる
Private_Dirty:         4 kB     0 kB    0 kB
Referenced:            4 kB     0 kB    4 kB   // unreferenced
Anonymous:             4 kB     4 kB    4 kB
LazyFree:              0 kB     0 kB    0 kB   // なぜか上がらない?
AnonHugePages:         0 kB     0 kB    0 kB
ShmemPmdMapped:        0 kB     0 kB    0 kB
FilePmdMapped:        0 kB      0 kB    0 kB
Shared_Hugetlb:        0 kB     0 kB    0 kB
Private_Hugetlb:       0 kB     0 kB    0 kB
Swap:                  0 kB     0 kB    0 kB   // swap はされない
SwapPss:               0 kB     0 kB    0 kB
~~~
これだけだとメモリ回収圧の延長で削除されたかわからないですね。Ubuntu 18.04 の 4.15 系カーネルでやると上記の "LazyFree" が計上されているのが確認できたのですが、最新の 5.4 では同じテストプログラムでは計上されていませんでした。今回時間切れで `MADV_FREE` の特徴を浮き彫りにするようなサンプルコードは用意できなかったので、今後余裕があったら (要望があったら) 追記という形にしようと思います。

## ソースコード

`MADV_FREE` は [`madvise_dontneed_free`](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L754) あたりが入り口となり、コールバック [`madvise_free_walk_ops`](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L439-L461) を用いて指定アドレス範囲を page table walk します。コールバックの内容は
[`mark_page_lazyfree`](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L673) の呼び出しです。

Lazy free というのは linux-mm ローカルな言葉ですが、具体的な動作は [`lru_lazyfree_fn`](https://github.com/torvalds/linux/blob/v5.4/mm/madvise.c#L559-L582) で行われています。通常 anonymous ページは PG_swapbacked というページフラグを用いてファイルのページキャッシュと区別されているのですが、lazy free なページはこのフラグがクリアされ、ページキャッシュ用の inactive LRU リストにつながれることによって pageout せず解放する処理を実現しているようです。この動作は[オリジナル実装](https://github.com/torvalds/linux/commit/854e9ed09dedf0c19ac8640e91bcc74bc3f9e5c9)ののち、[最適化として](https://github.com/torvalds/linux/commit/f7ad2a6cb9f7c4040004bedee84a70a9b985583e)として追加されたようですが、個人的には今たまたまそうなっている実装の隙間を利用したハックにみえて、メンテしやすさの面で少し危ういような気がします。

# MADV_WIPEONFORK

`MADV_WIPEONFORK` は v4.14 で追加されたフラグで、shared mapping されたメモリのデータが fork を通して子プロセスに見えてしまうことを防ぐというものです。[コミットログ](https://github.com/torvalds/linux/commit/d2cd9ede6e193dd7d88b6d27399e96229a551b19)にはサーバプロセスの乱数シードの秘匿や、暗号関連の処理において有用と説明されています。類似の `MADV_DONTFORK` (子プロセスから当該メモリにアクセスしようとする segmentation fault) と違って子プロセスからのアクセス時はゼロクリアされたメモリを見せる動作をします。

これは[簡単なプログラム](https://github.com/Naoya-Horiguchi/madvise_demo/blob/master/sample1.c)で動作確認できます。
~~~
$ ./sample1
mapped address: 0x700000000000
parent (3610), [test1]
child 1 (3611), [test1]
madvise(MADV_WIPEONFORK) returned 0
child 2 (3612), []                     // 子プロセスにメモリ内容が見えない。
madvise(MADV_KEEPONFORK) returned 0
child 3 (3613), [test1]
~~~

ソースコードレベルでは、[fork](https://github.com/torvalds/linux/blob/v5.4/kernel/fork.c#L600) の処理の延長で親プロセスから子プロセスへメモリをコピーする関数を呼び出すか否かを `VM_WIPONFORK` フラグで制御していることから、本フラグの効果は容易に理解できると思います。

# まとめ

ということで、4 つのページキャッシュ回収系の advice フラグの動作は永続化と回収タイミングの観点から下記のようにまとめられることがわかりました。

|                    | pageout/swapout | 破棄            |
|--------------------|-----------------|-----------------|
| メモリ回収圧発生時 | `MADV_COLD`     | `MADV_FREE`     |
| 即時               | `MADV_PAGEOUT`  | `MADV_DONTNEED` |

他にも、`MADV_FREE` と `MADV_WIPEONFORK` という比較的新しめの advice フラグについて解説しました。システムコールを直接呼び出すようなプログラムを書く人はそう多くはないと思いますが、こういった情報が誰かの役に立ったならば私としては幸いです。
