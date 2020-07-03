### Creating section

Sections form a tree, where the leaves are the only one allowed
to have content.

A leaf section is defined using `in=parent` and a block

A root section has no `in` and usually have a fixed size

Secttion layout options:
 First - Put this section first in the list of children
 Last - Keep this section last

Before any pass, clear the data of all sections

If section has no start, set from parent, otherwise keep

When the first pass is done, iterate over the tree and relocate
all sections after each other. If no overlaps occurs and nothing
was moved we are done.




```
; Root sections
!section "RAM" start=$0000 size=64*1024 NoStore
!section "ZP" start=$00 size=256 NoStore

; Non-leaf section
!section "text" in="RAM" NoStore{
    ; This can be put anywhere
}

; Leaf secton
!section "code" in="RAM", start=$801 {
    ; Basic start and such
    ; Another leaf section, defined recursively
    !section in="text" {
        name: !text "name"
    }
    lda name,x
}

!section "stuff" in="RAM" {
    ; This can be put anywhere
}


```


### Relocation process

* Go through the children of all parents
* Children must be sorted in order and not overlap
* First child is expected to start at parent start
* Lay out all children in order
* If any child was moved, we need another pass


