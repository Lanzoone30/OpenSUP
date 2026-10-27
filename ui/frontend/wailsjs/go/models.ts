export namespace config {
	
	export class Settings {
	    language: number;
	    theme: number;
	    last_bdn_dir: string;
	    last_output_dir: string;
	    params_open: boolean;
	    engine_open: boolean;
	    advanced_open: boolean;
	    allow_normal: boolean;
	    prefer_normal: boolean;
	    overlap: boolean;
	    full_palette: boolean;
	    alternate_oids: boolean;
	    both_formats: boolean;
	    ignore_resolution: boolean;
	    tight_ods: boolean;
	    single_object_ds: boolean;
	    quantizer: number;
	
	    static createFrom(source: any = {}) {
	        return new Settings(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.language = source["language"];
	        this.theme = source["theme"];
	        this.last_bdn_dir = source["last_bdn_dir"];
	        this.last_output_dir = source["last_output_dir"];
	        this.params_open = source["params_open"];
	        this.engine_open = source["engine_open"];
	        this.advanced_open = source["advanced_open"];
	        this.allow_normal = source["allow_normal"];
	        this.prefer_normal = source["prefer_normal"];
	        this.overlap = source["overlap"];
	        this.full_palette = source["full_palette"];
	        this.alternate_oids = source["alternate_oids"];
	        this.both_formats = source["both_formats"];
	        this.ignore_resolution = source["ignore_resolution"];
	        this.tight_ods = source["tight_ods"];
	        this.single_object_ds = source["single_object_ds"];
	        this.quantizer = source["quantizer"];
	    }
	}

}

export namespace engine {
	
	export class EncodeConfig {
	    InputPath: string;
	    OutputPath: string;
	    Quantizer: number;
	    BTMatrix: string;
	    Overwrite: boolean;
	    IgnoreResolution: boolean;
	    BothFormats: boolean;
	    FullPalette: boolean;
	    AllowNormalCase: boolean;
	    PreferNormalCase: boolean;
	    Overlap: boolean;
	    AlternateOids: boolean;
	    TightOds: boolean;
	    SingleObjectDs: boolean;
	    RedrawPeriod: number;
	    MaxKbps: number;
	    Threads: number;
	    Compression: number;
	    Acqrate: number;
	    SsimTol: number;
	    ExtraAcq: number;
	
	    static createFrom(source: any = {}) {
	        return new EncodeConfig(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.InputPath = source["InputPath"];
	        this.OutputPath = source["OutputPath"];
	        this.Quantizer = source["Quantizer"];
	        this.BTMatrix = source["BTMatrix"];
	        this.Overwrite = source["Overwrite"];
	        this.IgnoreResolution = source["IgnoreResolution"];
	        this.BothFormats = source["BothFormats"];
	        this.FullPalette = source["FullPalette"];
	        this.AllowNormalCase = source["AllowNormalCase"];
	        this.PreferNormalCase = source["PreferNormalCase"];
	        this.Overlap = source["Overlap"];
	        this.AlternateOids = source["AlternateOids"];
	        this.TightOds = source["TightOds"];
	        this.SingleObjectDs = source["SingleObjectDs"];
	        this.RedrawPeriod = source["RedrawPeriod"];
	        this.MaxKbps = source["MaxKbps"];
	        this.Threads = source["Threads"];
	        this.Compression = source["Compression"];
	        this.Acqrate = source["Acqrate"];
	        this.SsimTol = source["SsimTol"];
	        this.ExtraAcq = source["ExtraAcq"];
	    }
	}

}

