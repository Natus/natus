#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus-require.h>
#include <natus-require.hh>

namespace natus
{
  namespace require
  {
    struct HookMisc {
      void* misc;
      FreeFunction free;
      Hook func;

      static void
      freeit(void* hm)
      {
        delete (HookMisc*) hm;
      }

      static natusValue*
      translate(natusValue* ctx, natusRequireHookStep step, natusValue* name,
                natusValue* uri, void* misc)
      {
        if (!misc)
          return NULL;
        HookMisc* hm = (HookMisc*) misc;

        Value rslt = hm->func(Value(ctx, false),
                              (HookStep) step,
                              Value(name, false),
                              Value(uri, false),
                              hm->misc);
        return natus_incref(rslt.borrowCValue());
      }

      HookMisc(Hook fnc, void* m, FreeFunction f)
      {
        func = fnc;
        misc = m;
        free = f;
      }

      ~HookMisc()
      {
        if (misc && free)
          free(misc);
      }
    };

    bool
    addHook(Value ctx, const char* key, Hook func, void* misc, FreeFunction free)
    {
      return natus_require_hook_add(ctx.borrowCValue(), key, HookMisc::translate,
                                    new HookMisc(func, misc, free), HookMisc::freeit);
    }

    bool
    addOriginMatcher(Value ctx, const char* key, OriginMatcher func, void* misc, FreeFunction free)
    {
      return natus_require_origin_matcher_add(ctx.borrowCValue(), key, func, misc, free);
    }

    bool
    delHook(Value ctx, const char* key)
    {
      return natus_require_hook_del(ctx.borrowCValue(), key);
    }

    bool
    delOriginMatcher(Value ctx, const char* key)
    {
      return natus_require_origin_matcher_del(ctx.borrowCValue(), key);
    }

    bool
    init(Value ctx, const char* config)
    {
      return natus_require_init_utf8(ctx.borrowCValue(), config);
    }

    bool
    init(Value ctx, UTF8 config)
    {
      return natus_require_init_utf8(ctx.borrowCValue(), config.c_str());
    }

    bool
    init(Value ctx, Value config)
    {
      return natus_require_init(ctx.borrowCValue(), config.borrowCValue());
    }

    Value
    getConfig(Value ctx)
    {
      return natus_require_get_config(ctx.borrowCValue());
    }

    Value
    require(Value ctx, UTF8 name)
    {
      return natus_require_utf8(ctx.borrowCValue(), name.data());
    }

    Value
    require(Value ctx, UTF16 name)
    {
      return natus_require_utf16(ctx.borrowCValue(), name.data());
    }

    Value
    require(Value ctx, Value name)
    {
      return natus_require(ctx.borrowCValue(), name.borrowCValue());
    }

    bool
    originPermitted(Value ctx, const char* uri)
    {
      return natus_require_origin_permitted(ctx.borrowCValue(), uri);
    }
  }
}

