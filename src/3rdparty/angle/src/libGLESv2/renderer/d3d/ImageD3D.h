//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image.h: Defines the rx::Image class, an abstract base class for the
// renderer-specific classes which will define the interface to the underlying
// surfaces or resources.

#ifndef LIBGLESV2_RENDERER_IMAGED3D_H_
#define LIBGLESV2_RENDERER_IMAGED3D_H_

#include "common/debug.h"
#include "libGLESv2/renderer/Image.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{
class TextureStorage;

class ImageD3D : public Image
{
  public:
    ImageD3D();
    virtual ~ImageD3D() {};

    static ImageD3D *makeImageD3D(Image *img);

    virtual bool isDirty() const = 0;

    virtual void setManagedSurface2D(TextureStorage *storage, int level) {};
    virtual void setManagedSurfaceCube(TextureStorage *storage, int face, int level) {};
    virtual void setManagedSurface3D(TextureStorage *storage, int level) {};
    virtual void setManagedSurface2DArray(TextureStorage *storage, int layer, int level) {};
    virtual bool copyToStorage2D(TextureStorage *storage, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height) = 0;
    virtual bool copyToStorageCube(TextureStorage *storage, int face, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height) = 0;
    virtual bool copyToStorage3D(TextureStorage *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth) = 0;
    virtual bool copyToStorage2DArray(TextureStorage *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height) = 0;

  private:
    DISALLOW_COPY_AND_ASSIGN(ImageD3D);
};

}

#endif // LIBGLESV2_RENDERER_IMAGED3D_H_
