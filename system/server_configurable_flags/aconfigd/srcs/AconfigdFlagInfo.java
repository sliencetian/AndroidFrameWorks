/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.aconfigd;

import java.util.Objects;

/** @hide */
public class AconfigdFlagInfo {

    private String mPackageName;
    private String mFlagName;
    private String mServerFlagValue;
    private String mLocalFlagValue;
    private String mBootFlagValue;
    private String mDefaultFlagValue;
    private String mNamespace;
    private boolean mHasServerOverride;
    private boolean mHasLocalOverride;
    private boolean mIsReadWrite;

    AconfigdFlagInfo(Builder builder) {
        mPackageName = builder.mPackageName;
        mFlagName = builder.mFlagName;
        mServerFlagValue = builder.mServerFlagValue;
        mLocalFlagValue = builder.mLocalFlagValue;
        mDefaultFlagValue = builder.mDefaultFlagValue;
        mHasServerOverride = builder.mHasServerOverride;
        mHasLocalOverride = builder.mHasLocalOverride;
        mIsReadWrite = builder.mIsReadWrite;
        mBootFlagValue = builder.mBootFlagValue;
        if (mBootFlagValue == null) {
            updateBootFlagValue();
        }
        mNamespace = builder.mNamespace;
    }

    public String getFullFlagName() {
        StringBuilder ret = new StringBuilder(mPackageName);
        return ret.append('.').append(mFlagName).toString();
    }

    public String getPackageName() {
        return mPackageName;
    }

    public String getFlagName() {
        return mFlagName;
    }

    public String getServerFlagValue() {
        return mServerFlagValue;
    }

    public String getLocalFlagValue() {
        return mLocalFlagValue;
    }

    public String getBootFlagValue() {
        return mBootFlagValue;
    }

    public String getDefaultFlagValue() {
        return mDefaultFlagValue;
    }

    public String getNamespace() {
        return mNamespace;
    }

    public boolean getHasServerOverride() {
        return mHasServerOverride;
    }

    public boolean getHasLocalOverride() {
        return mHasLocalOverride;
    }

    public boolean getIsReadWrite() {
        return mIsReadWrite;
    }

    public void setLocalFlagValue(String localFlagValue) {
        if (!mIsReadWrite) {
            return;
        }
        mLocalFlagValue = localFlagValue;
        mHasLocalOverride = true;
        updateBootFlagValue();
    }

    public void setServerFlagValue(String serverFlagValue) {
        if (!mIsReadWrite) {
            return;
        }
        mServerFlagValue = serverFlagValue;
        mHasServerOverride = true;
        updateBootFlagValue();
    }

    private void updateBootFlagValue() {
        mBootFlagValue = mDefaultFlagValue == null ? mBootFlagValue : mDefaultFlagValue;
        if (mIsReadWrite) {
            mBootFlagValue = mServerFlagValue == null ? mBootFlagValue : mServerFlagValue;
            mBootFlagValue = mLocalFlagValue == null ? mBootFlagValue : mLocalFlagValue;
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof AconfigdFlagInfo)) {
            return false;
        }
        AconfigdFlagInfo other = (AconfigdFlagInfo) obj;
        return Objects.equals(mPackageName, other.mPackageName)
                && Objects.equals(mFlagName, other.mFlagName)
                && Objects.equals(mServerFlagValue, other.mServerFlagValue)
                && Objects.equals(mLocalFlagValue, other.mLocalFlagValue)
                && Objects.equals(mBootFlagValue, other.mBootFlagValue)
                && Objects.equals(mDefaultFlagValue, other.mDefaultFlagValue)
                && mHasServerOverride == other.mHasServerOverride
                && mHasLocalOverride == other.mHasLocalOverride
                && mIsReadWrite == other.mIsReadWrite;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mPackageName,
                mFlagName,
                mServerFlagValue,
                mLocalFlagValue,
                mBootFlagValue,
                mDefaultFlagValue,
                mHasServerOverride,
                mHasLocalOverride,
                mIsReadWrite);
    }

    public String dump() {
        return String.format(
                "packageName: %s, flagName: %s, serverFlagValue: %s, localFlagValue: %s,"
                        + " bootFlagValue: %s, defaultFlagValue: %s, hasServerOverride: %s,"
                        + " hasLocalOverride: %s, isReadWrite: %s",
                mPackageName,
                mFlagName,
                mServerFlagValue,
                mLocalFlagValue,
                mBootFlagValue,
                mDefaultFlagValue,
                mHasServerOverride,
                mHasLocalOverride,
                mIsReadWrite);
    }

    public String dumpDiff(AconfigdFlagInfo other) {
        StringBuilder ret = new StringBuilder();
        if (!Objects.equals(mPackageName, other.mPackageName)) {
            ret.append(String.format("packageName: %s -> %s\n", mPackageName, other.mPackageName));
            return ret.toString();
        }
        if (!Objects.equals(mFlagName, other.mFlagName)) {
            ret.append(String.format("flagName: %s -> %s\n", mFlagName, other.mFlagName));
            return ret.toString();
        }
        if (!Objects.equals(mBootFlagValue, other.mBootFlagValue)) {
            ret.append(
                    String.format(
                            "bootFlagValue: %s -> %s\n", mBootFlagValue, other.mBootFlagValue));
        }
        if (!Objects.equals(mDefaultFlagValue, other.mDefaultFlagValue)) {
            ret.append(
                    String.format(
                            "defaultFlagValue: %s -> %s\n",
                            mDefaultFlagValue, other.mDefaultFlagValue));
        }
        if (mIsReadWrite != other.mIsReadWrite) {
            ret.append(String.format("isReadWrite: %s -> %s\n", mIsReadWrite, other.mIsReadWrite));
        }
        if (mIsReadWrite && other.mIsReadWrite) {
            if (!Objects.equals(mServerFlagValue, other.mServerFlagValue)) {
                ret.append(
                        String.format(
                                "serverFlagValue: %s -> %s\n",
                                mServerFlagValue, other.mServerFlagValue));
            }
            if (!Objects.equals(mLocalFlagValue, other.mLocalFlagValue)) {
                ret.append(
                        String.format(
                                "localFlagValue: %s -> %s\n",
                                mLocalFlagValue, other.mLocalFlagValue));
            }
            if (mHasServerOverride != other.mHasServerOverride) {
                ret.append(
                        String.format(
                                "hasServerOverride: %s -> %s\n",
                                mHasServerOverride, other.mHasServerOverride));
            }
            if (mHasLocalOverride != other.mHasLocalOverride) {
                ret.append(
                        String.format(
                                "hasLocalOverride: %s -> %s\n",
                                mHasLocalOverride, other.mHasLocalOverride));
            }
        }
        return ret.toString();
    }

    public static Builder newBuilder() {
        return new Builder();
    }

    public static class Builder {
        private String mPackageName;
        private String mFlagName;
        private String mServerFlagValue;
        private String mLocalFlagValue;
        private String mBootFlagValue;
        private String mDefaultFlagValue;
        private String mNamespace;
        private boolean mHasServerOverride;
        private boolean mHasLocalOverride;
        private boolean mIsReadWrite;

        public Builder setPackageName(String packageName) {
            mPackageName = packageName;
            return this;
        }

        public Builder setFlagName(String flagName) {
            mFlagName = flagName;
            return this;
        }

        public Builder setServerFlagValue(String serverFlagValue) {
            mServerFlagValue = nullOrEmpty(serverFlagValue) ? null : serverFlagValue;
            return this;
        }

        public Builder setLocalFlagValue(String localFlagValue) {
            mLocalFlagValue = nullOrEmpty(localFlagValue) ? null : localFlagValue;
            return this;
        }

        public Builder setBootFlagValue(String bootFlagValue) {
            mBootFlagValue = nullOrEmpty(bootFlagValue) ? null : bootFlagValue;
            return this;
        }

        public Builder setDefaultFlagValue(String defaultFlagValue) {
            mDefaultFlagValue = nullOrEmpty(defaultFlagValue) ? null : defaultFlagValue;
            return this;
        }

        public Builder setNamespace(String namespace) {
            mNamespace = namespace;
            return this;
        }

        public Builder setHasServerOverride(boolean hasServerOverride) {
            mHasServerOverride = hasServerOverride;
            return this;
        }

        public Builder setHasLocalOverride(boolean hasLocalOverride) {
            mHasLocalOverride = hasLocalOverride;
            return this;
        }

        public Builder setIsReadWrite(boolean isReadWrite) {
            mIsReadWrite = isReadWrite;
            return this;
        }

        public AconfigdFlagInfo build() {
            return new AconfigdFlagInfo(this);
        }

        private boolean nullOrEmpty(String str) {
            return str == null || str.isEmpty();
        }
    }
}
