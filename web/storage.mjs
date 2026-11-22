export function openStorage(indexedDB) {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open("fodder-v1", 1);
    request.onupgradeneeded = () => {
      request.result.createObjectStore("profiles");
      request.result.createObjectStore("packs");
    };
    request.onerror = () => reject(request.error);
    request.onblocked = () => reject(new Error("Close other Fodder tabs to open saved data."));
    request.onsuccess = () => resolve(storage(request.result));
  });
}

function storage(db) {
  function read(store, select) {
    return new Promise((resolve, reject) => {
      const request = select(db.transaction(store, "readonly").objectStore(store));
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }

  function write(name, update) {
    return new Promise((resolve, reject) => {
      const transaction = db.transaction(name, "readwrite");
      const store = transaction.objectStore(name);
      let failure;
      transaction.oncomplete = () => resolve();
      transaction.onabort = () => reject(failure ?? transaction.error);
      update(store, (error) => {
        failure = error;
        transaction.abort();
      });
    });
  }

  return {
    async profile(id) {
      return (await read("profiles", (store) => store.get(id)))?.text ?? "";
    },
    saveProfile(id, text, revision) {
      return write("profiles", (store, abort) => {
        const existing = store.get(id);
        existing.onsuccess = () => {
          const saved = existing.result?.revision ?? 0;
          if (saved !== revision) {
            abort(new Error("Another tab changed this profile. Reload Fodder before continuing."));
          } else {
            store.put({ text, revision: revision + 1 }, id);
          }
        };
      });
    },
    allPacks() {
      return read("packs", (store) => store.getAll());
    },
    putPack(pack) {
      return write("packs", (store) => store.put(pack, pack.manifest.id));
    }
  };
}
