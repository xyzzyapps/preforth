/**
 * PreForth WebAssembly Runtime & Browser WASI Shim
 * https://github.com/xyzzyapps/preforth
 * PolyForm Noncommercial License 1.0.0
 */

class PreForthVM {
  constructor() {
    this.instance = null;
    this.memory = null;
    this.stdoutBuffer = '';
    this.stderrBuffer = '';
    this.onStdout = null;
    this.onStderr = null;
  }

  createWasiShim() {
    const self = this;
    return {
      fd_write(fd, iovs_ptr, iovs_len, nwritten_ptr) {
        if (!self.memory) return 0;
        const view = new DataView(self.memory.buffer);
        const u8 = new Uint8Array(self.memory.buffer);
        let total = 0;
        let str = '';
        for (let i = 0; i < iovs_len; i++) {
          const ptr = view.getUint32(iovs_ptr + i * 8, true);
          const len = view.getUint32(iovs_ptr + i * 8 + 4, true);
          str += new TextDecoder('utf-8').decode(u8.subarray(ptr, ptr + len));
          total += len;
        }
        view.setUint32(nwritten_ptr, total, true);
        if (fd === 1) {
          self.stdoutBuffer += str;
          if (self.onStdout) self.onStdout(str);
        } else if (fd === 2) {
          self.stderrBuffer += str;
          if (self.onStderr) self.onStderr(str);
        }
        return 0;
      },
      fd_close(fd) { return 0; },
      fd_fdstat_get(fd, stat_ptr) { return 0; },
      fd_fdstat_set_flags(fd, flags) { return 0; },
      fd_prestat_get(fd, buf_ptr) { return 8; /* BADF */ },
      fd_prestat_dir_name(fd, path_ptr, path_len) { return 8; },
      fd_read(fd, iovs, iovs_len, nread) { return 0; },
      fd_seek(fd, offset, whence, newoffset) { return 0; },
      path_open() { return 44; /* ENOENT */ },
      path_remove_directory() { return 44; },
      path_unlink_file() { return 44; },
      proc_exit(code) { console.warn('PreForth proc_exit called with code:', code); }
    };
  }

  async init(wasmSource = 'preforth.wasm') {
    const importObject = {
      wasi_snapshot_preview1: this.createWasiShim()
    };

    let wasmBytes;
    if (typeof wasmSource === 'string') {
      const response = await fetch(wasmSource);
      if (!response.ok) {
        throw new Error(`Failed to load ${wasmSource}: ${response.statusText}`);
      }
      wasmBytes = await response.arrayBuffer();
    } else if (wasmSource instanceof ArrayBuffer || wasmSource instanceof Uint8Array) {
      wasmBytes = wasmSource;
    } else {
      throw new Error('Unsupported wasmSource type');
    }

    const { instance } = await WebAssembly.instantiate(wasmBytes, importObject);
    this.instance = instance;
    this.memory = instance.exports.memory;

    if (typeof instance.exports._initialize === 'function') {
      instance.exports._initialize();
    }

    if (typeof instance.exports.preforth_wasm_init === 'function') {
      instance.exports.preforth_wasm_init();
    }

    return this;
  }

  reset() {
    if (!this.instance) return;
    this.stdoutBuffer = '';
    this.stderrBuffer = '';
    this.instance.exports.preforth_wasm_reset();
  }

  eval(code) {
    if (!this.instance) {
      throw new Error('PreForth VM is not initialized. Call init() first.');
    }

    this.stdoutBuffer = '';
    this.stderrBuffer = '';

    const encoder = new TextEncoder();
    const encoded = encoder.encode(code + '\0');
    const ptr = this.instance.exports.malloc(encoded.length);
    new Uint8Array(this.memory.buffer).set(encoded, ptr);

    const outPtr = this.instance.exports.preforth_wasm_eval(ptr);
    this.instance.exports.free(ptr);

    const u8 = new Uint8Array(this.memory.buffer);
    let end = outPtr;
    while (u8[end] !== 0) {
      end++;
    }
    const resultStr = new TextDecoder('utf-8').decode(u8.subarray(outPtr, end));
    return resultStr;
  }

  getStackDepth() {
    if (!this.instance) return 0;
    return this.instance.exports.preforth_wasm_stack_depth();
  }
}

// Global export for vanilla browser usage or module export
if (typeof window !== 'undefined') {
  window.PreForthVM = PreForthVM;
}
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { PreForthVM };
}
