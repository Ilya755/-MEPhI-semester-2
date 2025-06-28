from collections.abc import Callable, Hashable, Iterable
from collections import OrderedDict, namedtuple
from functools import wraps

def lru_cache(max_items: int) -> Callable:
    """
    Функция создает декоратор, позволяющий кэшировать результаты выполнения обернутой функции по принципу LRU-кэша.
    Размер LRU кэша ограничен количеством max_items. При попытке сохранить новый результат в кэш, в том случае, когда
    размер кэша уже равен max_size, происходит удаление одного из старых элементов, сохраненных в кэше.
    Удаляется тот элемент, к которому обращались давнее всего.
    Также у обернутой функции должен появиться атрибут stats, в котором лежит объект с атрибутами cache_hits и
    cache_misses, подсчитывающие количество успешных и неуспешных использований кэша.
    :param max_items: максимальный размер кэша.
    :return: декоратор, добавляющий LRU-кэширование для обернутой функции.
    """

    class LRUCache:
        cache_hits = 0
        cache_misses = 0

    def decorator(f):
        cache = OrderedDict()
        f.stats = LRUCache()
        @wraps(f)
        def wrapper(*args, **kwargs):
            key = args.__str__() + kwargs.__str__()
            if key in cache:
                f.stats.cache_hits += 1
                cache.move_to_end(key)
            else:
                if len(cache) == max_items:
                    cache.popitem(last=False)
                f.stats.cache_misses += 1
                val = f(*args, **kwargs)
                cache[key] = val
            return cache[key]
        return wrapper

    return decorator
