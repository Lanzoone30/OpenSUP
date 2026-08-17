export namespace config {
	
	export class Settings {
	    language: number;
	    theme: number;
	    last_bdn_dir: string;
	    last_output_dir: string;
	
	    static createFrom(source: any = {}) {
	        return new Settings(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.language = source["language"];
	        this.theme = source["theme"];
	        this.last_bdn_dir = source["last_bdn_dir"];
	        this.last_output_dir = source["last_output_dir"];
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
	    IgnoreRes: boolean;
	    BothFormats: boolean;
	    FullPalette: boolean;
	    AllowNormalCase: boolean;
	    PreferNormalCase: boolean;
	    Overlap: boolean;
	    RedrawPeriod: number;
	
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
	        this.IgnoreRes = source["IgnoreRes"];
	        this.BothFormats = source["BothFormats"];
	        this.FullPalette = source["FullPalette"];
	        this.AllowNormalCase = source["AllowNormalCase"];
	        this.PreferNormalCase = source["PreferNormalCase"];
	        this.Overlap = source["Overlap"];
	        this.RedrawPeriod = source["RedrawPeriod"];
	    }
	}

}

