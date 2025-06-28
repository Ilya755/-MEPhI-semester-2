class EnhancedList(list):
    """
    Улучшенный list.
    Данный класс является наследником класса list и добавляет к нему несколько новых атрибутов.

    - first -- позволяет получать и задавать значение первого элемента списка.
    - last -- позволяет получать и задавать значение последнего элемента списка.
    - size -- позволяет получать и задавать длину списка:
        - если новая длина больше старой, то дополнить список значениями None;
        - если новая длина меньше старой, то удалять значения из конца списка.
    """

    @property
    def first(self):
        if len(self) == 0:
            return None
        else:
            return self[0]

    @first.setter
    def first(self, value):
        self[0] = value

    @property
    def last(self):
        if len(self) == 0:
            return None
        else:
            return self[-1]

    @last.setter
    def last(self, value):
        self[-1] = value

    @property
    def size(self):
        return len(self)

    @size.setter
    def size(self, new_size):
        if new_size is None:
            raise ValueError("New size cannot be None")
        elif new_size > len(self):
            self.extend([None] * (new_size - len(self)))
        elif new_size < len(self):
            del self[new_size:]


    # def __getattr__(self, name):
    #     if name == "first":
    #         if len(self) == 0:
    #             return None
    #         else:
    #             return self[0]
    #     elif name == "last":
    #         if len(self) == 0:
    #             return None
    #         else:
    #             return self[-1]
    #     elif name == "size":
    #         return len(self)
    #     else:
    #         raise AttributeError(f"{type(self).__name__} object has no attribute {name}")
    #
    # def __setattr__(self, name, value):
    #     if name == "first":
    #         self[0] = value
    #     elif name == "last":
    #         self[-1] = value
    #     elif name == "size":
    #         if value > len(self):
    #             self.extend([None] * (value - len(self)))
    #         elif value < len(self):
    #             del self[value:]
    #     else:
    #         raise AttributeError(f"{type(self).__name__} object has no attribute {name}")