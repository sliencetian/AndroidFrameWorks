/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=java -Weverything -Wno-missing-permission-annotation -Werror -t --min_sdk_version platform_apis --ninja -d out/soong/.intermediates/system/tools/aidl/aidl-test-interface-java-source/gen/android/aidl/tests/FixedSize.java.d -o out/soong/.intermediates/system/tools/aidl/aidl-test-interface-java-source/gen -Nsystem/tools/aidl/tests system/tools/aidl/tests/android/aidl/tests/FixedSize.aidl
 */
package android.aidl.tests;
public class FixedSize implements android.os.Parcelable
{
  public static final android.os.Parcelable.Creator<FixedSize> CREATOR = new android.os.Parcelable.Creator<FixedSize>() {
    @Override
    public FixedSize createFromParcel(android.os.Parcel _aidl_source) {
      FixedSize _aidl_out = new FixedSize();
      _aidl_out.readFromParcel(_aidl_source);
      return _aidl_out;
    }
    @Override
    public FixedSize[] newArray(int _aidl_size) {
      return new FixedSize[_aidl_size];
    }
  };
  @Override public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag)
  {
    int _aidl_start_pos = _aidl_parcel.dataPosition();
    _aidl_parcel.writeInt(0);
    int _aidl_end_pos = _aidl_parcel.dataPosition();
    _aidl_parcel.setDataPosition(_aidl_start_pos);
    _aidl_parcel.writeInt(_aidl_end_pos - _aidl_start_pos);
    _aidl_parcel.setDataPosition(_aidl_end_pos);
  }
  public final void readFromParcel(android.os.Parcel _aidl_parcel)
  {
    int _aidl_start_pos = _aidl_parcel.dataPosition();
    int _aidl_parcelable_size = _aidl_parcel.readInt();
    try {
      if (_aidl_parcelable_size < 4) throw new android.os.BadParcelableException("Parcelable too small");;
    } finally {
      if (_aidl_start_pos > (Integer.MAX_VALUE - _aidl_parcelable_size)) {
        throw new android.os.BadParcelableException("Overflow in the size of parcelable");
      }
      _aidl_parcel.setDataPosition(_aidl_start_pos + _aidl_parcelable_size);
    }
  }
  @Override
  public int describeContents() {
    int _mask = 0;
    return _mask;
  }
  public static class FixedParcelable implements android.os.Parcelable
  {
    public boolean booleanValue = false;
    public byte byteValue = 0;
    public char charValue = '\0';
    public int intValue = 0;
    public long longValue = 0L;
    public float floatValue = 0.000000f;
    public int[] intArray;
    public long[][] multiDimensionLongArray;
    public double doubleValue = 0.000000;
    public long enumValue = android.aidl.tests.LongEnum.FOO;
    public android.aidl.tests.FixedSize.FixedUnion parcelableValue;
    public android.aidl.tests.FixedSize.EmptyParcelable[] parcelableArray;
    public android.aidl.tests.FixedSize.FixedUnion[] unionArray;
    public static final android.os.Parcelable.Creator<FixedParcelable> CREATOR = new android.os.Parcelable.Creator<FixedParcelable>() {
      @Override
      public FixedParcelable createFromParcel(android.os.Parcel _aidl_source) {
        FixedParcelable _aidl_out = new FixedParcelable();
        _aidl_out.readFromParcel(_aidl_source);
        return _aidl_out;
      }
      @Override
      public FixedParcelable[] newArray(int _aidl_size) {
        return new FixedParcelable[_aidl_size];
      }
    };
    @Override public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag)
    {
      int _aidl_start_pos = _aidl_parcel.dataPosition();
      _aidl_parcel.writeInt(0);
      _aidl_parcel.writeBoolean(booleanValue);
      _aidl_parcel.writeByte(byteValue);
      _aidl_parcel.writeInt(((int)charValue));
      _aidl_parcel.writeInt(intValue);
      _aidl_parcel.writeLong(longValue);
      _aidl_parcel.writeFloat(floatValue);
      _aidl_parcel.writeFixedArray(intArray, _aidl_flag, 3);
      _aidl_parcel.writeFixedArray(multiDimensionLongArray, _aidl_flag, 3, 2);
      _aidl_parcel.writeDouble(doubleValue);
      _aidl_parcel.writeLong(enumValue);
      _aidl_parcel.writeTypedObject(parcelableValue, _aidl_flag);
      _aidl_parcel.writeFixedArray(parcelableArray, _aidl_flag, 3);
      _aidl_parcel.writeFixedArray(unionArray, _aidl_flag, 4);
      int _aidl_end_pos = _aidl_parcel.dataPosition();
      _aidl_parcel.setDataPosition(_aidl_start_pos);
      _aidl_parcel.writeInt(_aidl_end_pos - _aidl_start_pos);
      _aidl_parcel.setDataPosition(_aidl_end_pos);
    }
    public final void readFromParcel(android.os.Parcel _aidl_parcel)
    {
      int _aidl_start_pos = _aidl_parcel.dataPosition();
      int _aidl_parcelable_size = _aidl_parcel.readInt();
      try {
        if (_aidl_parcelable_size < 4) throw new android.os.BadParcelableException("Parcelable too small");;
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        booleanValue = _aidl_parcel.readBoolean();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        byteValue = _aidl_parcel.readByte();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        charValue = (char)_aidl_parcel.readInt();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        intValue = _aidl_parcel.readInt();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        longValue = _aidl_parcel.readLong();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        floatValue = _aidl_parcel.readFloat();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        intArray = _aidl_parcel.createFixedArray(int[].class, 3);
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        multiDimensionLongArray = _aidl_parcel.createFixedArray(long[][].class, 3, 2);
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        doubleValue = _aidl_parcel.readDouble();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        enumValue = _aidl_parcel.readLong();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        parcelableValue = _aidl_parcel.readTypedObject(android.aidl.tests.FixedSize.FixedUnion.CREATOR);
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        parcelableArray = _aidl_parcel.createFixedArray(android.aidl.tests.FixedSize.EmptyParcelable[].class, android.aidl.tests.FixedSize.EmptyParcelable.CREATOR, 3);
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        unionArray = _aidl_parcel.createFixedArray(android.aidl.tests.FixedSize.FixedUnion[].class, android.aidl.tests.FixedSize.FixedUnion.CREATOR, 4);
      } finally {
        if (_aidl_start_pos > (Integer.MAX_VALUE - _aidl_parcelable_size)) {
          throw new android.os.BadParcelableException("Overflow in the size of parcelable");
        }
        _aidl_parcel.setDataPosition(_aidl_start_pos + _aidl_parcelable_size);
      }
    }
    @Override
    public int describeContents() {
      int _mask = 0;
      _mask |= describeContents(parcelableValue);
      _mask |= describeContents(parcelableArray);
      _mask |= describeContents(unionArray);
      return _mask;
    }
    private int describeContents(Object _v) {
      if (_v == null) return 0;
      if (_v instanceof Object[]) {
        int _mask = 0;
        for (Object o : (Object[]) _v) {
          _mask |= describeContents(o);
        }
        return _mask;
      }
      if (_v instanceof android.os.Parcelable) {
        return ((android.os.Parcelable) _v).describeContents();
      }
      return 0;
    }
  }
  /**
   * long and double are only aligned to 4 bytes on x86 so the generated rust
   * structs need explicit padding for field offsets to match across archs
   */
  public static class ExplicitPaddingParcelable implements android.os.Parcelable
  {
    public byte byteValue = 0;
    /** rust has an explicit [u8; 7] for padding here */
    public long longValue = 0L;
    public char charValue = '\0';
    /** rust has an explicit [u8; 6] for padding here */
    public double doubleValue = 0.000000;
    public int intValue = 0;
    /**
     * We align enums correctly despite the backing type so there's no
     * explicit padding here
     */
    public long enumValue = android.aidl.tests.LongEnum.FOO;
    public static final android.os.Parcelable.Creator<ExplicitPaddingParcelable> CREATOR = new android.os.Parcelable.Creator<ExplicitPaddingParcelable>() {
      @Override
      public ExplicitPaddingParcelable createFromParcel(android.os.Parcel _aidl_source) {
        ExplicitPaddingParcelable _aidl_out = new ExplicitPaddingParcelable();
        _aidl_out.readFromParcel(_aidl_source);
        return _aidl_out;
      }
      @Override
      public ExplicitPaddingParcelable[] newArray(int _aidl_size) {
        return new ExplicitPaddingParcelable[_aidl_size];
      }
    };
    @Override public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag)
    {
      int _aidl_start_pos = _aidl_parcel.dataPosition();
      _aidl_parcel.writeInt(0);
      _aidl_parcel.writeByte(byteValue);
      _aidl_parcel.writeLong(longValue);
      _aidl_parcel.writeInt(((int)charValue));
      _aidl_parcel.writeDouble(doubleValue);
      _aidl_parcel.writeInt(intValue);
      _aidl_parcel.writeLong(enumValue);
      int _aidl_end_pos = _aidl_parcel.dataPosition();
      _aidl_parcel.setDataPosition(_aidl_start_pos);
      _aidl_parcel.writeInt(_aidl_end_pos - _aidl_start_pos);
      _aidl_parcel.setDataPosition(_aidl_end_pos);
    }
    public final void readFromParcel(android.os.Parcel _aidl_parcel)
    {
      int _aidl_start_pos = _aidl_parcel.dataPosition();
      int _aidl_parcelable_size = _aidl_parcel.readInt();
      try {
        if (_aidl_parcelable_size < 4) throw new android.os.BadParcelableException("Parcelable too small");;
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        byteValue = _aidl_parcel.readByte();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        longValue = _aidl_parcel.readLong();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        charValue = (char)_aidl_parcel.readInt();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        doubleValue = _aidl_parcel.readDouble();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        intValue = _aidl_parcel.readInt();
        if (_aidl_parcel.dataPosition() - _aidl_start_pos >= _aidl_parcelable_size) return;
        enumValue = _aidl_parcel.readLong();
      } finally {
        if (_aidl_start_pos > (Integer.MAX_VALUE - _aidl_parcelable_size)) {
          throw new android.os.BadParcelableException("Overflow in the size of parcelable");
        }
        _aidl_parcel.setDataPosition(_aidl_start_pos + _aidl_parcelable_size);
      }
    }
    @Override
    public int describeContents() {
      int _mask = 0;
      return _mask;
    }
  }
  /**
   * The NDK backend generates an empty struct which is 1 byte in C++ so the
   * rust side must generate a struct with a u8 field
   */
  public static class EmptyParcelable implements android.os.Parcelable
  {
    public static final android.os.Parcelable.Creator<EmptyParcelable> CREATOR = new android.os.Parcelable.Creator<EmptyParcelable>() {
      @Override
      public EmptyParcelable createFromParcel(android.os.Parcel _aidl_source) {
        EmptyParcelable _aidl_out = new EmptyParcelable();
        _aidl_out.readFromParcel(_aidl_source);
        return _aidl_out;
      }
      @Override
      public EmptyParcelable[] newArray(int _aidl_size) {
        return new EmptyParcelable[_aidl_size];
      }
    };
    @Override public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag)
    {
      int _aidl_start_pos = _aidl_parcel.dataPosition();
      _aidl_parcel.writeInt(0);
      int _aidl_end_pos = _aidl_parcel.dataPosition();
      _aidl_parcel.setDataPosition(_aidl_start_pos);
      _aidl_parcel.writeInt(_aidl_end_pos - _aidl_start_pos);
      _aidl_parcel.setDataPosition(_aidl_end_pos);
    }
    public final void readFromParcel(android.os.Parcel _aidl_parcel)
    {
      int _aidl_start_pos = _aidl_parcel.dataPosition();
      int _aidl_parcelable_size = _aidl_parcel.readInt();
      try {
        if (_aidl_parcelable_size < 4) throw new android.os.BadParcelableException("Parcelable too small");;
      } finally {
        if (_aidl_start_pos > (Integer.MAX_VALUE - _aidl_parcelable_size)) {
          throw new android.os.BadParcelableException("Overflow in the size of parcelable");
        }
        _aidl_parcel.setDataPosition(_aidl_start_pos + _aidl_parcelable_size);
      }
    }
    @Override
    public int describeContents() {
      int _mask = 0;
      return _mask;
    }
  }
  public static final class FixedUnion implements android.os.Parcelable {
    // tags for union fields
    public final static int booleanValue = 0;  // boolean booleanValue;
    public final static int byteValue = 1;  // byte byteValue;
    public final static int charValue = 2;  // char charValue;
    public final static int intValue = 3;  // int intValue;
    public final static int longValue = 4;  // long longValue;
    public final static int floatValue = 5;  // float floatValue;
    public final static int intArray = 6;  // int[3] intArray;
    public final static int multiDimensionLongArray = 7;  // long[3][2] multiDimensionLongArray;
    public final static int doubleValue = 8;  // double doubleValue;
    public final static int enumValue = 9;  // android.aidl.tests.LongEnum enumValue;

    private int _tag;
    private Object _value;

    public FixedUnion() {
      boolean _value = false;
      this._tag = booleanValue;
      this._value = _value;
    }

    private FixedUnion(android.os.Parcel _aidl_parcel) {
      readFromParcel(_aidl_parcel);
    }

    private FixedUnion(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }

    public int getTag() {
      return _tag;
    }

    // boolean booleanValue;

    public static FixedUnion booleanValue(boolean _value) {
      return new FixedUnion(booleanValue, _value);
    }

    public boolean getBooleanValue() {
      _assertTag(booleanValue);
      return (boolean) _value;
    }

    public void setBooleanValue(boolean _value) {
      _set(booleanValue, _value);
    }

    // byte byteValue;

    public static FixedUnion byteValue(byte _value) {
      return new FixedUnion(byteValue, _value);
    }

    public byte getByteValue() {
      _assertTag(byteValue);
      return (byte) _value;
    }

    public void setByteValue(byte _value) {
      _set(byteValue, _value);
    }

    // char charValue;

    public static FixedUnion charValue(char _value) {
      return new FixedUnion(charValue, _value);
    }

    public char getCharValue() {
      _assertTag(charValue);
      return (char) _value;
    }

    public void setCharValue(char _value) {
      _set(charValue, _value);
    }

    // int intValue;

    public static FixedUnion intValue(int _value) {
      return new FixedUnion(intValue, _value);
    }

    public int getIntValue() {
      _assertTag(intValue);
      return (int) _value;
    }

    public void setIntValue(int _value) {
      _set(intValue, _value);
    }

    // long longValue;

    public static FixedUnion longValue(long _value) {
      return new FixedUnion(longValue, _value);
    }

    public long getLongValue() {
      _assertTag(longValue);
      return (long) _value;
    }

    public void setLongValue(long _value) {
      _set(longValue, _value);
    }

    // float floatValue;

    public static FixedUnion floatValue(float _value) {
      return new FixedUnion(floatValue, _value);
    }

    public float getFloatValue() {
      _assertTag(floatValue);
      return (float) _value;
    }

    public void setFloatValue(float _value) {
      _set(floatValue, _value);
    }

    // int[3] intArray;

    public static FixedUnion intArray(int[] _value) {
      return new FixedUnion(intArray, _value);
    }

    public int[] getIntArray() {
      _assertTag(intArray);
      return (int[]) _value;
    }

    public void setIntArray(int[] _value) {
      _set(intArray, _value);
    }

    // long[3][2] multiDimensionLongArray;

    public static FixedUnion multiDimensionLongArray(long[][] _value) {
      return new FixedUnion(multiDimensionLongArray, _value);
    }

    public long[][] getMultiDimensionLongArray() {
      _assertTag(multiDimensionLongArray);
      return (long[][]) _value;
    }

    public void setMultiDimensionLongArray(long[][] _value) {
      _set(multiDimensionLongArray, _value);
    }

    // double doubleValue;

    public static FixedUnion doubleValue(double _value) {
      return new FixedUnion(doubleValue, _value);
    }

    public double getDoubleValue() {
      _assertTag(doubleValue);
      return (double) _value;
    }

    public void setDoubleValue(double _value) {
      _set(doubleValue, _value);
    }

    // android.aidl.tests.LongEnum enumValue;

    public static FixedUnion enumValue(long _value) {
      return new FixedUnion(enumValue, _value);
    }

    public long getEnumValue() {
      _assertTag(enumValue);
      return (long) _value;
    }

    public void setEnumValue(long _value) {
      _set(enumValue, _value);
    }

    public static final android.os.Parcelable.Creator<FixedUnion> CREATOR = new android.os.Parcelable.Creator<FixedUnion>() {
      @Override
      public FixedUnion createFromParcel(android.os.Parcel _aidl_source) {
        return new FixedUnion(_aidl_source);
      }
      @Override
      public FixedUnion[] newArray(int _aidl_size) {
        return new FixedUnion[_aidl_size];
      }
    };

    @Override
    public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag) {
      _aidl_parcel.writeInt(_tag);
      switch (_tag) {
      case booleanValue:
        _aidl_parcel.writeBoolean(getBooleanValue());
        break;
      case byteValue:
        _aidl_parcel.writeByte(getByteValue());
        break;
      case charValue:
        _aidl_parcel.writeInt(((int)getCharValue()));
        break;
      case intValue:
        _aidl_parcel.writeInt(getIntValue());
        break;
      case longValue:
        _aidl_parcel.writeLong(getLongValue());
        break;
      case floatValue:
        _aidl_parcel.writeFloat(getFloatValue());
        break;
      case intArray:
        _aidl_parcel.writeFixedArray(getIntArray(), _aidl_flag, 3);
        break;
      case multiDimensionLongArray:
        _aidl_parcel.writeFixedArray(getMultiDimensionLongArray(), _aidl_flag, 3, 2);
        break;
      case doubleValue:
        _aidl_parcel.writeDouble(getDoubleValue());
        break;
      case enumValue:
        _aidl_parcel.writeLong(getEnumValue());
        break;
      }
    }

    public void readFromParcel(android.os.Parcel _aidl_parcel) {
      int _aidl_tag;
      _aidl_tag = _aidl_parcel.readInt();
      switch (_aidl_tag) {
      case booleanValue: {
        boolean _aidl_value;
        _aidl_value = _aidl_parcel.readBoolean();
        _set(_aidl_tag, _aidl_value);
        return; }
      case byteValue: {
        byte _aidl_value;
        _aidl_value = _aidl_parcel.readByte();
        _set(_aidl_tag, _aidl_value);
        return; }
      case charValue: {
        char _aidl_value;
        _aidl_value = (char)_aidl_parcel.readInt();
        _set(_aidl_tag, _aidl_value);
        return; }
      case intValue: {
        int _aidl_value;
        _aidl_value = _aidl_parcel.readInt();
        _set(_aidl_tag, _aidl_value);
        return; }
      case longValue: {
        long _aidl_value;
        _aidl_value = _aidl_parcel.readLong();
        _set(_aidl_tag, _aidl_value);
        return; }
      case floatValue: {
        float _aidl_value;
        _aidl_value = _aidl_parcel.readFloat();
        _set(_aidl_tag, _aidl_value);
        return; }
      case intArray: {
        int[] _aidl_value;
        _aidl_value = _aidl_parcel.createFixedArray(int[].class, 3);
        _set(_aidl_tag, _aidl_value);
        return; }
      case multiDimensionLongArray: {
        long[][] _aidl_value;
        _aidl_value = _aidl_parcel.createFixedArray(long[][].class, 3, 2);
        _set(_aidl_tag, _aidl_value);
        return; }
      case doubleValue: {
        double _aidl_value;
        _aidl_value = _aidl_parcel.readDouble();
        _set(_aidl_tag, _aidl_value);
        return; }
      case enumValue: {
        long _aidl_value;
        _aidl_value = _aidl_parcel.readLong();
        _set(_aidl_tag, _aidl_value);
        return; }
      }
      throw new IllegalArgumentException("union: unknown tag: " + _aidl_tag);
    }

    @Override
    public int describeContents() {
      int _mask = 0;
      switch (getTag()) {
      }
      return _mask;
    }

    private void _assertTag(int tag) {
      if (getTag() != tag) {
        throw new IllegalStateException("bad access: " + _tagString(tag) + ", " + _tagString(getTag()) + " is available.");
      }
    }

    private String _tagString(int _tag) {
      switch (_tag) {
      case booleanValue: return "booleanValue";
      case byteValue: return "byteValue";
      case charValue: return "charValue";
      case intValue: return "intValue";
      case longValue: return "longValue";
      case floatValue: return "floatValue";
      case intArray: return "intArray";
      case multiDimensionLongArray: return "multiDimensionLongArray";
      case doubleValue: return "doubleValue";
      case enumValue: return "enumValue";
      }
      throw new IllegalStateException("unknown field: " + _tag);
    }

    private void _set(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }
    public static @interface Tag {
      public static final byte booleanValue = 0;
      public static final byte byteValue = 1;
      public static final byte charValue = 2;
      public static final byte intValue = 3;
      public static final byte longValue = 4;
      public static final byte floatValue = 5;
      public static final byte intArray = 6;
      public static final byte multiDimensionLongArray = 7;
      public static final byte doubleValue = 8;
      public static final byte enumValue = 9;
    }
  }
  /** A union with no padding between the tag and value */
  public static final class FixedUnionNoPadding implements android.os.Parcelable {
    // tags for union fields
    public final static int byteValue = 0;  // byte byteValue;

    private int _tag;
    private Object _value;

    public FixedUnionNoPadding() {
      byte _value = 0;
      this._tag = byteValue;
      this._value = _value;
    }

    private FixedUnionNoPadding(android.os.Parcel _aidl_parcel) {
      readFromParcel(_aidl_parcel);
    }

    private FixedUnionNoPadding(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }

    public int getTag() {
      return _tag;
    }

    // byte byteValue;

    public static FixedUnionNoPadding byteValue(byte _value) {
      return new FixedUnionNoPadding(byteValue, _value);
    }

    public byte getByteValue() {
      _assertTag(byteValue);
      return (byte) _value;
    }

    public void setByteValue(byte _value) {
      _set(byteValue, _value);
    }

    public static final android.os.Parcelable.Creator<FixedUnionNoPadding> CREATOR = new android.os.Parcelable.Creator<FixedUnionNoPadding>() {
      @Override
      public FixedUnionNoPadding createFromParcel(android.os.Parcel _aidl_source) {
        return new FixedUnionNoPadding(_aidl_source);
      }
      @Override
      public FixedUnionNoPadding[] newArray(int _aidl_size) {
        return new FixedUnionNoPadding[_aidl_size];
      }
    };

    @Override
    public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag) {
      _aidl_parcel.writeInt(_tag);
      switch (_tag) {
      case byteValue:
        _aidl_parcel.writeByte(getByteValue());
        break;
      }
    }

    public void readFromParcel(android.os.Parcel _aidl_parcel) {
      int _aidl_tag;
      _aidl_tag = _aidl_parcel.readInt();
      switch (_aidl_tag) {
      case byteValue: {
        byte _aidl_value;
        _aidl_value = _aidl_parcel.readByte();
        _set(_aidl_tag, _aidl_value);
        return; }
      }
      throw new IllegalArgumentException("union: unknown tag: " + _aidl_tag);
    }

    @Override
    public int describeContents() {
      int _mask = 0;
      switch (getTag()) {
      }
      return _mask;
    }

    private void _assertTag(int tag) {
      if (getTag() != tag) {
        throw new IllegalStateException("bad access: " + _tagString(tag) + ", " + _tagString(getTag()) + " is available.");
      }
    }

    private String _tagString(int _tag) {
      switch (_tag) {
      case byteValue: return "byteValue";
      }
      throw new IllegalStateException("unknown field: " + _tag);
    }

    private void _set(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }
    public static @interface Tag {
      public static final byte byteValue = 0;
    }
  }
  /** A union with one byte of padding between the tag and value */
  public static final class FixedUnionSmallPadding implements android.os.Parcelable {
    // tags for union fields
    public final static int charValue = 0;  // char charValue;

    private int _tag;
    private Object _value;

    public FixedUnionSmallPadding() {
      char _value = '\0';
      this._tag = charValue;
      this._value = _value;
    }

    private FixedUnionSmallPadding(android.os.Parcel _aidl_parcel) {
      readFromParcel(_aidl_parcel);
    }

    private FixedUnionSmallPadding(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }

    public int getTag() {
      return _tag;
    }

    // char charValue;

    public static FixedUnionSmallPadding charValue(char _value) {
      return new FixedUnionSmallPadding(charValue, _value);
    }

    public char getCharValue() {
      _assertTag(charValue);
      return (char) _value;
    }

    public void setCharValue(char _value) {
      _set(charValue, _value);
    }

    public static final android.os.Parcelable.Creator<FixedUnionSmallPadding> CREATOR = new android.os.Parcelable.Creator<FixedUnionSmallPadding>() {
      @Override
      public FixedUnionSmallPadding createFromParcel(android.os.Parcel _aidl_source) {
        return new FixedUnionSmallPadding(_aidl_source);
      }
      @Override
      public FixedUnionSmallPadding[] newArray(int _aidl_size) {
        return new FixedUnionSmallPadding[_aidl_size];
      }
    };

    @Override
    public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag) {
      _aidl_parcel.writeInt(_tag);
      switch (_tag) {
      case charValue:
        _aidl_parcel.writeInt(((int)getCharValue()));
        break;
      }
    }

    public void readFromParcel(android.os.Parcel _aidl_parcel) {
      int _aidl_tag;
      _aidl_tag = _aidl_parcel.readInt();
      switch (_aidl_tag) {
      case charValue: {
        char _aidl_value;
        _aidl_value = (char)_aidl_parcel.readInt();
        _set(_aidl_tag, _aidl_value);
        return; }
      }
      throw new IllegalArgumentException("union: unknown tag: " + _aidl_tag);
    }

    @Override
    public int describeContents() {
      int _mask = 0;
      switch (getTag()) {
      }
      return _mask;
    }

    private void _assertTag(int tag) {
      if (getTag() != tag) {
        throw new IllegalStateException("bad access: " + _tagString(tag) + ", " + _tagString(getTag()) + " is available.");
      }
    }

    private String _tagString(int _tag) {
      switch (_tag) {
      case charValue: return "charValue";
      }
      throw new IllegalStateException("unknown field: " + _tag);
    }

    private void _set(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }
    public static @interface Tag {
      public static final byte charValue = 0;
    }
  }
  /** A union with seven bytes of padding between the tag and value */
  public static final class FixedUnionLongPadding implements android.os.Parcelable {
    // tags for union fields
    public final static int longValue = 0;  // long longValue;

    private int _tag;
    private Object _value;

    public FixedUnionLongPadding() {
      long _value = 0L;
      this._tag = longValue;
      this._value = _value;
    }

    private FixedUnionLongPadding(android.os.Parcel _aidl_parcel) {
      readFromParcel(_aidl_parcel);
    }

    private FixedUnionLongPadding(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }

    public int getTag() {
      return _tag;
    }

    // long longValue;

    public static FixedUnionLongPadding longValue(long _value) {
      return new FixedUnionLongPadding(longValue, _value);
    }

    public long getLongValue() {
      _assertTag(longValue);
      return (long) _value;
    }

    public void setLongValue(long _value) {
      _set(longValue, _value);
    }

    public static final android.os.Parcelable.Creator<FixedUnionLongPadding> CREATOR = new android.os.Parcelable.Creator<FixedUnionLongPadding>() {
      @Override
      public FixedUnionLongPadding createFromParcel(android.os.Parcel _aidl_source) {
        return new FixedUnionLongPadding(_aidl_source);
      }
      @Override
      public FixedUnionLongPadding[] newArray(int _aidl_size) {
        return new FixedUnionLongPadding[_aidl_size];
      }
    };

    @Override
    public final void writeToParcel(android.os.Parcel _aidl_parcel, int _aidl_flag) {
      _aidl_parcel.writeInt(_tag);
      switch (_tag) {
      case longValue:
        _aidl_parcel.writeLong(getLongValue());
        break;
      }
    }

    public void readFromParcel(android.os.Parcel _aidl_parcel) {
      int _aidl_tag;
      _aidl_tag = _aidl_parcel.readInt();
      switch (_aidl_tag) {
      case longValue: {
        long _aidl_value;
        _aidl_value = _aidl_parcel.readLong();
        _set(_aidl_tag, _aidl_value);
        return; }
      }
      throw new IllegalArgumentException("union: unknown tag: " + _aidl_tag);
    }

    @Override
    public int describeContents() {
      int _mask = 0;
      switch (getTag()) {
      }
      return _mask;
    }

    private void _assertTag(int tag) {
      if (getTag() != tag) {
        throw new IllegalStateException("bad access: " + _tagString(tag) + ", " + _tagString(getTag()) + " is available.");
      }
    }

    private String _tagString(int _tag) {
      switch (_tag) {
      case longValue: return "longValue";
      }
      throw new IllegalStateException("unknown field: " + _tag);
    }

    private void _set(int _tag, Object _value) {
      this._tag = _tag;
      this._value = _value;
    }
    public static @interface Tag {
      public static final byte longValue = 0;
    }
  }
}
