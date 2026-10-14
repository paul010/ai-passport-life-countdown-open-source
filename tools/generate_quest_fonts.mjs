// Reproducible UI-only subset. Install lv_font_conv 1.5.3 once; subsequent
// generation is offline. All glyphs remain in Flash, not a full runtime font.
import {readFileSync,writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const root=new URL('../',import.meta.url);
const source=new URL('assets/fonts/NotoSansCJKsc-Regular.otf',root);
const text=['main/life_quest_app.c','main/life_quest_ui.c']
  .map(path=>readFileSync(new URL(path,root),'utf8')).join('');
const discovered=[...new Set([...text].filter(c=>c.codePointAt(0)>127))].sort().join('');
const recorded=JSON.parse(readFileSync(new URL('assets/fonts/quest-glyphs.json',root),'utf8'));
const symbols=process.argv.includes('--update-glyphs') ? discovered : recorded.symbols;
for(const size of [11,14,18]) {
 const name=`life_quest_font_${size}`;
 const r=spawnSync('npm',['exec','--offline','--yes','--package=lv_font_conv@1.5.3','--',
  'lv_font_conv','--font',source.pathname,'--size',String(size),'--bpp','2',
  '--format','lvgl','--range','0x20-0x7e','--symbols',symbols,'--no-compress',
  '--no-kerning','--lv-include','lvgl.h','--lv-font-name',name,'-o',new URL(`assets/fonts/${name}.c`,root).pathname],
  {stdio:'inherit'});
 if(r.status!==0)process.exit(r.status??1);
 const output=new URL(`assets/fonts/${name}.c`,root);
 // Normalize converter whitespace without changing any glyph data.
 writeFileSync(output,readFileSync(output,'utf8').replaceAll(root.pathname, '').trimEnd()+'\n');
}
writeFileSync(new URL('assets/fonts/quest-glyphs.json',root),JSON.stringify({
 source:'NotoSansCJKsc-Regular.otf',source_sha256:createHash('sha256').update(readFileSync(source)).digest('hex'),
 converter:'lv_font_conv@1.5.3',sizes:[11,14,18],bpp:2,ascii:[32,126],symbols,
},null,2)+'\n');
