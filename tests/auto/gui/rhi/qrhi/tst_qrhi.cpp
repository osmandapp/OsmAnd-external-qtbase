/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QThread>
#include <QFile>
#include <QOffscreenSurface>
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhinull_p.h>

#if QT_CONFIG(opengl)
# include <QOpenGLContext>
# include <QtGui/private/qrhigles2_p.h>
# define TST_GL
#endif

#if QT_CONFIG(vulkan)
# include <QVulkanInstance>
# include <QtGui/private/qrhivulkan_p.h>
# define TST_VK
#endif

#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
# define TST_D3D11
#endif

#ifdef Q_OS_DARWIN
# include <QtGui/private/qrhimetal_p.h>
# define TST_MTL
#endif

Q_DECLARE_METATYPE(QRhi::Implementation)
Q_DECLARE_METATYPE(QRhiInitParams *)

class tst_QRhi : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void rhiTestData();
    void create_data();
    void create();
    void nativeHandles_data();
    void nativeHandles();
    void resourceUpdateBatchBuffer_data();
    void resourceUpdateBatchBuffer();

private:
    struct {
        QRhiNullInitParams null;
#ifdef TST_GL
        QRhiGles2InitParams gl;
#endif
#ifdef TST_VK
        QRhiVulkanInitParams vk;
#endif
#ifdef TST_D3D11
        QRhiD3D11InitParams d3d;
#endif
#ifdef TST_MTL
        QRhiMetalInitParams mtl;
#endif
    } initParams;

#ifdef TST_VK
    QVulkanInstance vulkanInstance;
#endif
    QOffscreenSurface *fallbackSurface = nullptr;
};

void tst_QRhi::initTestCase()
{
#ifdef TST_GL
    fallbackSurface = QRhiGles2InitParams::newFallbackSurface();
    initParams.gl.fallbackSurface = fallbackSurface;
#endif

#ifdef TST_VK
#ifndef Q_OS_ANDROID
    vulkanInstance.setLayers({ QByteArrayLiteral("VK_LAYER_LUNARG_standard_validation") });
#else
    vulkanInstance.setLayers({ QByteArrayLiteral("VK_LAYER_GOOGLE_threading"),
                               QByteArrayLiteral("VK_LAYER_LUNARG_parameter_validation"),
                               QByteArrayLiteral("VK_LAYER_LUNARG_object_tracker"),
                               QByteArrayLiteral("VK_LAYER_LUNARG_core_validation"),
                               QByteArrayLiteral("VK_LAYER_LUNARG_image"),
                               QByteArrayLiteral("VK_LAYER_LUNARG_swapchain"),
                               QByteArrayLiteral("VK_LAYER_GOOGLE_unique_objects") });
#endif
    vulkanInstance.setExtensions(QByteArrayList()
                                 << "VK_KHR_get_physical_device_properties2");
    vulkanInstance.create();
    initParams.vk.inst = &vulkanInstance;
#endif

#ifdef TST_D3D11
    initParams.d3d.enableDebugLayer = true;
#endif
}

void tst_QRhi::cleanupTestCase()
{
#ifdef TST_VK
    vulkanInstance.destroy();
#endif

    delete fallbackSurface;
}

void tst_QRhi::rhiTestData()
{
    QTest::addColumn<QRhi::Implementation>("impl");
    QTest::addColumn<QRhiInitParams *>("initParams");

    QTest::newRow("Null") << QRhi::Null << static_cast<QRhiInitParams *>(&initParams.null);
#ifdef TST_GL
    QTest::newRow("OpenGL") << QRhi::OpenGLES2 << static_cast<QRhiInitParams *>(&initParams.gl);
#endif
#ifdef TST_VK
    if (vulkanInstance.isValid())
        QTest::newRow("Vulkan") << QRhi::Vulkan << static_cast<QRhiInitParams *>(&initParams.vk);
#endif
#ifdef TST_D3D11
    QTest::newRow("Direct3D 11") << QRhi::D3D11 << static_cast<QRhiInitParams *>(&initParams.d3d);
#endif
#ifdef TST_MTL
    QTest::newRow("Metal") << QRhi::Metal << static_cast<QRhiInitParams *>(&initParams.mtl);
#endif
}

void tst_QRhi::create_data()
{
    rhiTestData();
}

static int aligned(int v, int a)
{
    return (v + a - 1) & ~(a - 1);
}

void tst_QRhi::create()
{
    // Merely attempting to create a QRhi should survive, with an error when
    // not supported. (of course, there is always a chance we encounter a crash
    // due to some random graphics stack...)

    QFETCH(QRhi::Implementation, impl);
    QFETCH(QRhiInitParams *, initParams);

    QScopedPointer<QRhi> rhi(QRhi::create(impl, initParams, QRhi::Flags(), nullptr));

    if (rhi) {
        QCOMPARE(rhi->backend(), impl);
        QCOMPARE(rhi->thread(), QThread::currentThread());

        // do a basic smoke test for the apis that do not directly render anything

        int cleanupOk = 0;
        QRhi *rhiPtr = rhi.data();
        auto cleanupFunc = [rhiPtr, &cleanupOk](QRhi *dyingRhi) {
            if (rhiPtr == dyingRhi)
                cleanupOk += 1;
        };
        rhi->addCleanupCallback(cleanupFunc);
        rhi->runCleanup();
        QCOMPARE(cleanupOk, 1);
        cleanupOk = 0;
        rhi->addCleanupCallback(cleanupFunc);

        QRhiResourceUpdateBatch *resUpd = rhi->nextResourceUpdateBatch();
        QVERIFY(resUpd);
        resUpd->release();

        QVERIFY(!rhi->supportedSampleCounts().isEmpty());
        QVERIFY(rhi->supportedSampleCounts().contains(1));

        QVERIFY(rhi->ubufAlignment() > 0);
        QCOMPARE(rhi->ubufAligned(123), aligned(123, rhi->ubufAlignment()));

        QCOMPARE(rhi->mipLevelsForSize(QSize(512, 300)), 10);
        QCOMPARE(rhi->sizeForMipLevel(0, QSize(512, 300)), QSize(512, 300));
        QCOMPARE(rhi->sizeForMipLevel(1, QSize(512, 300)), QSize(256, 150));
        QCOMPARE(rhi->sizeForMipLevel(2, QSize(512, 300)), QSize(128, 75));
        QCOMPARE(rhi->sizeForMipLevel(9, QSize(512, 300)), QSize(1, 1));

        const bool fbUp = rhi->isYUpInFramebuffer();
        const bool ndcUp = rhi->isYUpInNDC();
        const bool d0to1 = rhi->isClipDepthZeroToOne();
        const QMatrix4x4 corrMat = rhi->clipSpaceCorrMatrix();
        if (impl == QRhi::OpenGLES2) {
            QVERIFY(fbUp);
            QVERIFY(ndcUp);
            QVERIFY(!d0to1);
            QVERIFY(corrMat.isIdentity());
        } else if (impl == QRhi::Vulkan) {
            QVERIFY(!fbUp);
            QVERIFY(!ndcUp);
            QVERIFY(d0to1);
            QVERIFY(!corrMat.isIdentity());
        } else if (impl == QRhi::D3D11) {
            QVERIFY(!fbUp);
            QVERIFY(ndcUp);
            QVERIFY(d0to1);
            QVERIFY(!corrMat.isIdentity());
        } else if (impl == QRhi::Metal) {
            QVERIFY(!fbUp);
            QVERIFY(ndcUp);
            QVERIFY(d0to1);
            QVERIFY(!corrMat.isIdentity());
        }

        const int texMin = rhi->resourceLimit(QRhi::TextureSizeMin);
        const int texMax = rhi->resourceLimit(QRhi::TextureSizeMax);
        const int maxAtt = rhi->resourceLimit(QRhi::MaxColorAttachments);
        const int framesInFlight = rhi->resourceLimit(QRhi::FramesInFlight);
        QVERIFY(texMin >= 1);
        QVERIFY(texMax >= texMin);
        QVERIFY(maxAtt >= 1);
        QVERIFY(framesInFlight >= 1);

        QVERIFY(rhi->nativeHandles());
        QVERIFY(rhi->profiler());

        const QRhi::Feature features[] = {
            QRhi::MultisampleTexture,
            QRhi::MultisampleRenderBuffer,
            QRhi::DebugMarkers,
            QRhi::Timestamps,
            QRhi::Instancing,
            QRhi::CustomInstanceStepRate,
            QRhi::PrimitiveRestart,
            QRhi::NonDynamicUniformBuffers,
            QRhi::NonFourAlignedEffectiveIndexBufferOffset,
            QRhi::NPOTTextureRepeat,
            QRhi::RedOrAlpha8IsRed,
            QRhi::ElementIndexUint,
            QRhi::Compute,
            QRhi::WideLines,
            QRhi::VertexShaderPointSize,
            QRhi::BaseVertex,
            QRhi::BaseInstance,
            QRhi::TriangleFanTopology,
            QRhi::ReadBackNonUniformBuffer
        };
        for (size_t i = 0; i <sizeof(features) / sizeof(QRhi::Feature); ++i)
            rhi->isFeatureSupported(features[i]);

        QVERIFY(rhi->isTextureFormatSupported(QRhiTexture::RGBA8));

        rhi->releaseCachedResources();

        QVERIFY(!rhi->isDeviceLost());

        rhi.reset();
        QCOMPARE(cleanupOk, 1);
    }
}

void tst_QRhi::nativeHandles_data()
{
    rhiTestData();
}

void tst_QRhi::nativeHandles()
{
    QFETCH(QRhi::Implementation, impl);
    QFETCH(QRhiInitParams *, initParams);

    QScopedPointer<QRhi> rhi(QRhi::create(impl, initParams, QRhi::Flags(), nullptr));
    if (!rhi)
        QSKIP("QRhi could not be created, skipping testing native handles");

    // QRhi::nativeHandles()
    {
        const QRhiNativeHandles *rhiHandles = rhi->nativeHandles();
        Q_ASSERT(rhiHandles);

        switch (impl) {
        case QRhi::Null:
            break;
#ifdef TST_VK
        case QRhi::Vulkan:
        {
            const QRhiVulkanNativeHandles *vkHandles = static_cast<const QRhiVulkanNativeHandles *>(rhiHandles);
            QVERIFY(vkHandles->physDev);
            QVERIFY(vkHandles->dev);
            QVERIFY(vkHandles->gfxQueueFamilyIdx >= 0);
            QVERIFY(vkHandles->gfxQueue);
            QVERIFY(vkHandles->cmdPool);
            QVERIFY(vkHandles->vmemAllocator);
        }
            break;
#endif
#ifdef TST_GL
        case QRhi::OpenGLES2:
        {
            const QRhiGles2NativeHandles *glHandles = static_cast<const QRhiGles2NativeHandles *>(rhiHandles);
            QVERIFY(glHandles->context);
            QVERIFY(glHandles->context->isValid());
            glHandles->context->doneCurrent();
            QVERIFY(!QOpenGLContext::currentContext());
            rhi->makeThreadLocalNativeContextCurrent();
            QVERIFY(QOpenGLContext::currentContext() == glHandles->context);
        }
            break;
#endif
#ifdef TST_D3D11
        case QRhi::D3D11:
        {
            const QRhiD3D11NativeHandles *d3dHandles = static_cast<const QRhiD3D11NativeHandles *>(rhiHandles);
            QVERIFY(d3dHandles->dev);
            QVERIFY(d3dHandles->context);
        }
            break;
#endif
#ifdef TST_MTL
        case QRhi::Metal:
        {
            const QRhiMetalNativeHandles *mtlHandles = static_cast<const QRhiMetalNativeHandles *>(rhiHandles);
            QVERIFY(mtlHandles->dev);
            QVERIFY(mtlHandles->cmdQueue);
        }
            break;
#endif
        default:
            Q_ASSERT(false);
        }
    }

    // QRhiTexture::nativeHandles()
    {
        QScopedPointer<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, QSize(512, 256)));
        QVERIFY(tex->build());

        const QRhiNativeHandles *texHandles = tex->nativeHandles();
        QVERIFY(texHandles);

        switch (impl) {
        case QRhi::Null:
            break;
#ifdef TST_VK
        case QRhi::Vulkan:
        {
            const QRhiVulkanTextureNativeHandles *vkHandles = static_cast<const QRhiVulkanTextureNativeHandles *>(texHandles);
            QVERIFY(vkHandles->image);
            QVERIFY(vkHandles->layout >= 1); // VK_IMAGE_LAYOUT_GENERAL
            QVERIFY(vkHandles->layout <= 8); // VK_IMAGE_LAYOUT_PREINITIALIZED
        }
            break;
#endif
#ifdef TST_GL
        case QRhi::OpenGLES2:
        {
            const QRhiGles2TextureNativeHandles *glHandles = static_cast<const QRhiGles2TextureNativeHandles *>(texHandles);
            QVERIFY(glHandles->texture);
        }
            break;
#endif
#ifdef TST_D3D11
        case QRhi::D3D11:
        {
            const QRhiD3D11TextureNativeHandles *d3dHandles = static_cast<const QRhiD3D11TextureNativeHandles *>(texHandles);
            QVERIFY(d3dHandles->texture);
        }
            break;
#endif
#ifdef TST_MTL
        case QRhi::Metal:
        {
            const QRhiMetalTextureNativeHandles *mtlHandles = static_cast<const QRhiMetalTextureNativeHandles *>(texHandles);
            QVERIFY(mtlHandles->texture);
        }
            break;
#endif
        default:
            Q_ASSERT(false);
        }
    }

    // QRhiCommandBuffer::nativeHandles()
    {
        QRhiCommandBuffer *cb = nullptr;
        QRhi::FrameOpResult result = rhi->beginOffscreenFrame(&cb);
        QVERIFY(result == QRhi::FrameOpSuccess);
        QVERIFY(cb);

        const QRhiNativeHandles *cbHandles = cb->nativeHandles();
        // no null check here, backends where not applicable will return null

        switch (impl) {
        case QRhi::Null:
            break;
#ifdef TST_VK
        case QRhi::Vulkan:
        {
            const QRhiVulkanCommandBufferNativeHandles *vkHandles = static_cast<const QRhiVulkanCommandBufferNativeHandles *>(cbHandles);
            QVERIFY(vkHandles);
            QVERIFY(vkHandles->commandBuffer);
        }
            break;
#endif
#ifdef TST_GL
        case QRhi::OpenGLES2:
            break;
#endif
#ifdef TST_D3D11
        case QRhi::D3D11:
            break;
#endif
#ifdef TST_MTL
        case QRhi::Metal:
        {
            const QRhiMetalCommandBufferNativeHandles *mtlHandles = static_cast<const QRhiMetalCommandBufferNativeHandles *>(cbHandles);
            QVERIFY(mtlHandles);
            QVERIFY(mtlHandles->commandBuffer);
            QVERIFY(!mtlHandles->encoder);

            QScopedPointer<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, QSize(512, 512), 1, QRhiTexture::RenderTarget));
            QVERIFY(tex->build());
            QScopedPointer<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget({ tex.data() }));
            QScopedPointer<QRhiRenderPassDescriptor> rpDesc(rt->newCompatibleRenderPassDescriptor());
            QVERIFY(rpDesc);
            rt->setRenderPassDescriptor(rpDesc.data());
            QVERIFY(rt->build());
            cb->beginPass(rt.data(), Qt::red, { 1.0f, 0 });
            QVERIFY(static_cast<const QRhiMetalCommandBufferNativeHandles *>(cb->nativeHandles())->encoder);
            cb->endPass();
        }
            break;
#endif
        default:
            Q_ASSERT(false);
        }

        rhi->endOffscreenFrame();
    }

    // QRhiRenderPassDescriptor::nativeHandles()
    {
        QScopedPointer<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, QSize(512, 512), 1, QRhiTexture::RenderTarget));
        QVERIFY(tex->build());
        QScopedPointer<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget({ tex.data() }));
        QScopedPointer<QRhiRenderPassDescriptor> rpDesc(rt->newCompatibleRenderPassDescriptor());
        QVERIFY(rpDesc);
        rt->setRenderPassDescriptor(rpDesc.data());
        QVERIFY(rt->build());

        const QRhiNativeHandles *rpHandles = rpDesc->nativeHandles();
        switch (impl) {
        case QRhi::Null:
            break;
#ifdef TST_VK
        case QRhi::Vulkan:
        {
            const QRhiVulkanRenderPassNativeHandles *vkHandles = static_cast<const QRhiVulkanRenderPassNativeHandles *>(rpHandles);
            QVERIFY(vkHandles);
            QVERIFY(vkHandles->renderPass);
        }
            break;
#endif
#ifdef TST_GL
        case QRhi::OpenGLES2:
            break;
#endif
#ifdef TST_D3D11
        case QRhi::D3D11:
            break;
#endif
#ifdef TST_MTL
        case QRhi::Metal:
            break;
#endif
        default:
            Q_ASSERT(false);
        }
    }
}

void tst_QRhi::resourceUpdateBatchBuffer_data()
{
    rhiTestData();
}

void tst_QRhi::resourceUpdateBatchBuffer()
{
    QFETCH(QRhi::Implementation, impl);
    QFETCH(QRhiInitParams *, initParams);

    QScopedPointer<QRhi> rhi(QRhi::create(impl, initParams, QRhi::Flags(), nullptr));
    if (!rhi)
        QSKIP("QRhi could not be created, skipping testing resource updates");

    const int bufferSize = 23;
    const QByteArray a(bufferSize, 'A');
    const QByteArray b(bufferSize, 'B');

    // dynamic buffer, updates, readback
    {
        QScopedPointer<QRhiBuffer> dynamicBuffer(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, bufferSize));
        QVERIFY(dynamicBuffer->build());

        QRhiResourceUpdateBatch *batch = rhi->nextResourceUpdateBatch();
        QVERIFY(batch);

        batch->updateDynamicBuffer(dynamicBuffer.data(), 10, bufferSize - 10, a.constData());
        batch->updateDynamicBuffer(dynamicBuffer.data(), 0, 12, b.constData());

        QRhiBufferReadbackResult readResult;
        bool readCompleted = false;
        readResult.completed = [&readCompleted] { readCompleted = true; };
        batch->readBackBuffer(dynamicBuffer.data(), 5, 10, &readResult);

        QRhiCommandBuffer *cb = nullptr;
        QRhi::FrameOpResult result = rhi->beginOffscreenFrame(&cb);
        QVERIFY(result == QRhi::FrameOpSuccess);
        QVERIFY(cb);
        cb->resourceUpdate(batch);
        rhi->endOffscreenFrame();

        // Offscreen frames are synchronous, so the readback must have
        // completed at this point. With swapchain frames this would not be the
        // case.
        QVERIFY(readCompleted);
        QVERIFY(readResult.data.size() == 10);
        QCOMPARE(readResult.data.left(7), QByteArrayLiteral("BBBBBBB"));
        QCOMPARE(readResult.data.mid(7), QByteArrayLiteral("AAA"));
    }

    // static buffer, updates, readback
    {
        QScopedPointer<QRhiBuffer> dynamicBuffer(rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer, bufferSize));
        QVERIFY(dynamicBuffer->build());

        QRhiResourceUpdateBatch *batch = rhi->nextResourceUpdateBatch();
        QVERIFY(batch);

        batch->uploadStaticBuffer(dynamicBuffer.data(), 10, bufferSize - 10, a.constData());
        batch->uploadStaticBuffer(dynamicBuffer.data(), 0, 12, b.constData());

        QRhiBufferReadbackResult readResult;
        bool readCompleted = false;
        readResult.completed = [&readCompleted] { readCompleted = true; };

        if (rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
            batch->readBackBuffer(dynamicBuffer.data(), 5, 10, &readResult);

        QRhiCommandBuffer *cb = nullptr;
        QRhi::FrameOpResult result = rhi->beginOffscreenFrame(&cb);
        QVERIFY(result == QRhi::FrameOpSuccess);
        QVERIFY(cb);
        cb->resourceUpdate(batch);
        rhi->endOffscreenFrame();

        if (rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer)) {
            QVERIFY(readCompleted);
            QVERIFY(readResult.data.size() == 10);
            QCOMPARE(readResult.data.left(7), QByteArrayLiteral("BBBBBBB"));
            QCOMPARE(readResult.data.mid(7), QByteArrayLiteral("AAA"));
        } else {
            qDebug("Skipping verifying buffer contents because readback is not supported");
        }
    }
}

#include <tst_qrhi.moc>
QTEST_MAIN(tst_QRhi)
