use std::marker::PhantomData;

/// User Defined Synchronization Object
pub struct UserSync<'a> {
    pub(super) handle: nvtx_sys::SyncUserHandle,
    pub(super) _lifetime: PhantomData<&'a ()>,
}

impl<'a> UserSync<'a> {
    /// Signal to tools that an attempt to acquire a user defined synchronization object.
    ///
    /// ```
    /// let d = nvtx::Domain::new("domain");
    /// let us = d.user_sync("custom object");
    /// // ...
    /// let started = us.acquire();
    /// ```
    #[must_use = "Dropping the return will violate the state machine"]
    pub fn acquire(self) -> UserSyncAcquireStart<'a> {
        nvtx_sys::domain_syncuser_acquire_start(self.handle);
        UserSyncAcquireStart { sync_object: self }
    }
}

impl Drop for UserSync<'_> {
    fn drop(&mut self) {
        nvtx_sys::domain_syncuser_destroy(self.handle)
    }
}

/// State modeling the start of acquiring the synchronization object.
pub struct UserSyncAcquireStart<'a> {
    sync_object: UserSync<'a>,
}

impl<'a> UserSyncAcquireStart<'a> {
    /// Signal to tools of failure in acquiring a user defined synchronization object.
    ///
    /// ```
    /// let d = nvtx::Domain::new("domain");
    /// let us = d.user_sync("custom object");
    /// // ...
    /// let started = us.acquire();
    /// // ...
    /// let us2 = started.failed();
    /// ```
    #[must_use = "Dropping the return will result in the Synchronization Object being destroyed"]
    pub fn failed(self) -> UserSync<'a> {
        nvtx_sys::domain_syncuser_acquire_failed(self.sync_object.handle);
        self.sync_object
    }

    /// Signal to tools of success in acquiring a user defined synchronization object.
    ///
    /// ```
    /// let d = nvtx::Domain::new("domain");
    /// let us = d.user_sync("custom object");
    /// // ...
    /// let started = us.acquire();
    /// // ...
    /// let success = started.success();
    /// ```
    #[must_use = "Dropping the return will violate the state machine"]
    pub fn success(self) -> UserSyncSuccess<'a> {
        nvtx_sys::domain_syncuser_acquire_success(self.sync_object.handle);
        UserSyncSuccess {
            sync_object: self.sync_object,
        }
    }
}

/// State modeling the success of acquiring the synchronization object.
pub struct UserSyncSuccess<'a> {
    sync_object: UserSync<'a>,
}

impl<'a> UserSyncSuccess<'a> {
    /// Signal to tools of releasing a reservation on user defined synchronization object.
    ///
    /// ```
    /// let d = nvtx::Domain::new("domain");
    /// let us = d.user_sync("custom object");
    /// // ...
    /// let started = us.acquire();
    /// // ...
    /// let success = started.success();
    /// // ...
    /// let us2 = success.release();
    /// ```
    #[must_use = "Dropping the return will result in the Synchronization Object being destroyed"]
    pub fn release(self) -> UserSync<'a> {
        nvtx_sys::domain_syncuser_acquire_releasing(self.sync_object.handle);
        self.sync_object
    }
}
