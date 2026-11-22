# Play using touch and immersive WebXR

## Design

The browser maps DOM pointer events and tracked controller state onto the shared C inputs. Immersive frames share one simulation tick across views, then render each viewport to the XR framebuffer.
