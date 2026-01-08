// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

public final class PrismInitProvider extends ContentProvider {
  @Override
  public boolean onCreate() {
    PrismContext.set(getContext());
    return true;
  }

  @Override
  public Cursor query(Uri uri, String[] p, String s, String[] a, String so) {
    return null;
  }

  @Override
  public String getType(Uri uri) {
    return null;
  }

  @Override
  public Uri insert(Uri uri, ContentValues v) {
    return null;
  }

  @Override
  public int delete(Uri uri, String s, String[] a) {
    return 0;
  }

  @Override
  public int update(Uri uri, ContentValues v, String s, String[] a) {
    return 0;
  }
}
