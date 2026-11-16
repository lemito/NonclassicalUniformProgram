import re, zipfile, html
from pathlib import Path
from xml.sax.saxutils import escape

SRC = Path('docs/course_report.md')
OUT = Path('docs/course_report_gost.docx')

W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
M='http://schemas.openxmlformats.org/officeDocument/2006/math'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
CT='http://schemas.openxmlformats.org/package/2006/content-types'
PKG='http://schemas.openxmlformats.org/package/2006/relationships'

def q(ns, tag): return f'{{{ns}}}{tag}'

def E(tag, text=None, attrs=None, ns=W, children=None):
    a = ' '.join(f'{k}="{escape(str(v))}"' for k,v in (attrs or {}).items())
    s = f'<{tag}' + ((' '+a) if a else '') + '>'
    if text is not None: s += escape(text)
    if children: s += ''.join(children)
    return s + f'</{tag}>'

def empty(tag, attrs=None, ns=W):
    a=' '.join(f'{k}="{escape(str(v))}"' for k,v in (attrs or {}).items())
    return f'<{tag}' + ((' '+a) if a else '') + '/>'

def rpr(bold=False, italic=False, size=28, font='Times New Roman'):
    parts=[E('w:rFonts',attrs={'w:ascii':font,'w:hAnsi':font,'w:eastAsia':font}),E('w:sz',attrs={'w:val':size}),E('w:szCs',attrs={'w:val':size})]
    if bold: parts.append(empty('w:b'))
    if italic: parts.append(empty('w:i'))
    return E('w:rPr',children=parts)

def run(text, bold=False, italic=False, size=28):
    return E('w:r',children=[rpr(bold,italic,size), E('w:t', text=text)])

def p(text='', align=None, bold=False, italic=False, style=None, first=True, keep=False, pagebreak=False):
    ppr=[]
    if style: ppr.append(E('w:pStyle',attrs={'w:val':style}))
    if align: ppr.append(E('w:jc',attrs={'w:val':align}))
    elif style not in ('Heading1','Heading2','Heading3'): ppr.append(E('w:jc',attrs={'w:val':'both'}))
    if first: ppr.append(E('w:ind',attrs={'w:firstLine':'709'}))
    ppr.append(E('w:spacing',attrs={'w:line':'360','w:lineRule':'auto','w:after':'0'}))
    if keep: ppr.append(empty('w:keepNext'))
    if pagebreak: ppr.append(empty('w:pageBreakBefore'))
    content = E('w:pPr',children=ppr)
    if text:
        # basic markdown inline
        pos=0
        for m in re.finditer(r'\*\*(.+?)\*\*|`(.+?)`', text):
            if m.start()>pos: content += run(text[pos:m.start()])
            content += run(m.group(1) or m.group(2), bold=bool(m.group(1)))
            pos=m.end()
        if pos<len(text): content += run(text[pos:])
    return E('w:p',children=[content])

def formula_paragraph(tex):
    # Editable OMML object. The linear presentation is intentionally kept close to source notation.
    t=tex.strip()
    t=t.replace('\\,',' ').replace('\\quad','    ').replace('\\rightarrow','→').replace('\\Rightarrow','⇒')
    repl={'\\operatorname{supp}':'supp','\\operatorname{core}':'core','\\operatorname{sign}':'sign','\\mathbb{R}':'ℝ','\\mathbb{N}':'ℕ','\\alpha':'α','\\beta':'β','\\gamma':'γ','\\delta':'δ','\\mu':'μ','\\sqrt':'√','\\leq':'≤','\\geq':'≥','\\neq':'≠','\\notin':'∉','\\sum':'Σ','\\int':'∫','\\in':'∈','\\sup':'sup','\\min':'min','\\max':'max','\\prod':'Π','\\to':'→','\\mathbin{\\gg}':'≫'}
    for a,b in repl.items(): t=t.replace(a,b)
    t=t.replace('^2','²').replace('^3','³').replace('^\\circ','°').replace(')2', ')·2')
    t=re.sub(r'\\frac\{([^{}]+)\}\{([^{}]+)\}',r'(\1)/(\2)',t)
    t=re.sub(r'\\text\{([^{}]+)\}',r'\1',t)
    t=re.sub(r'\\[a-zA-Z]+','',t)
    t=t.replace('{','').replace('}','')
    sub = str.maketrans({'0':'₀','1':'₁','2':'₂','3':'₃','4':'₄','5':'₅','6':'₆','7':'₇','8':'₈','9':'₉','a':'ₐ','e':'ₑ','h':'ₕ','i':'ᵢ','j':'ⱼ','k':'ₖ','l':'ₗ','m':'ₘ','n':'ₙ','o':'ₒ','p':'ₚ','r':'ᵣ','s':'ₛ','t':'ₜ','u':'ᵤ','v':'ᵥ','x':'ₓ','y':'ᵧ'})
    t=re.sub(r'([A-Za-zα-ω])_([A-Za-z0-9α-ω]+)', lambda m: m.group(1)+''.join(ch.translate(sub) for ch in m.group(2)), t)
    t=re.sub(r'\^\{(-?\d+)\}', lambda m: ''.join(ch.translate(str.maketrans('-0123456789','⁻⁰¹²³⁴⁵⁶⁷⁸⁹')) for ch in m.group(1)), t)
    t=re.sub(r'\^(-?\d+)', lambda m: ''.join(ch.translate(str.maketrans('-0123456789','⁻⁰¹²³⁴⁵⁶⁷⁸⁹')) for ch in m.group(1)), t)
    omml = '<m:oMathPara xmlns:m="%s"><m:oMath><m:r><m:t>%s</m:t></m:r></m:oMath></m:oMathPara>' % (M, escape(t))
    ppr=E('w:pPr',children=[E('w:jc',attrs={'w:val':'center'}),E('w:spacing',attrs={'w:line':'360','w:after':'120'})])
    return '<w:p xmlns:w="%s" xmlns:m="%s">%s%s</w:p>'%(W,M,ppr,omml)

def bullet(text, level=0):
    ppr=E('w:pPr',children=[E('w:numPr',children=[E('w:ilvl',attrs={'w:val':level}),E('w:numId',attrs={'w:val':'1'})]),E('w:spacing',attrs={'w:line':'360','w:after':'0'})])
    return E('w:p',children=[ppr,run(text)])

def number_item(text):
    ppr=E('w:pPr',children=[E('w:numPr',children=[E('w:ilvl',attrs={'w:val':'0'}),E('w:numId',attrs={'w:val':'2'})]),E('w:spacing',attrs={'w:line':'360','w:after':'0'})])
    return E('w:p',children=[ppr,run(text)])

def heading(text, level):
    sty={1:'Heading1',2:'Heading2',3:'Heading3'}.get(level,'Heading3')
    return p(text, align='left', style=sty, first=False, keep=True, pagebreak=(level==1))

def table(rows):
    cols=max(len(r) for r in rows)
    widths=[int(9000/cols)]*cols
    out=[E('w:tblPr',children=[E('w:tblW',attrs={'w:w':'9000','w:type':'dxa'}),E('w:tblBorders',children=[E('w:top',attrs={'w:val':'single','w:sz':'4','w:color':'000000'}),E('w:left',attrs={'w:val':'single','w:sz':'4','w:color':'000000'}),E('w:bottom',attrs={'w:val':'single','w:sz':'4','w:color':'000000'}),E('w:right',attrs={'w:val':'single','w:sz':'4','w:color':'000000'}),E('w:insideH',attrs={'w:val':'single','w:sz':'4','w:color':'000000'}),E('w:insideV',attrs={'w:val':'single','w:sz':'4','w:color':'000000'})])])]
    for ri,row in enumerate(rows):
        cells=[]
        for c in range(cols):
            val=row[c] if c<len(row) else ''
            val=val.strip()
            shading=E('w:shd',attrs={'w:fill':'D9E2F3'}) if ri==0 else ''
            cp=E('w:tcPr',children=[E('w:tcW',attrs={'w:w':str(widths[c]),'w:type':'dxa'})]+([shading] if shading else []))
            cells.append(E('w:tc',children=[cp,p(val,align='center' if ri==0 else 'left',bold=(ri==0),first=False)]))
        out.append(E('w:tr',children=cells))
    return E('w:tbl',children=out)

md=SRC.read_text(encoding='utf-8').splitlines()
# title block is generated separately from реквизитов, so start body at annotation
try: md = md[next(i for i,line in enumerate(md) if line.strip()=='## Аннотация'):]
except StopIteration: pass
# markdown contents is replaced by a formatted static table of contents
try:
    a=next(i for i,line in enumerate(md) if line.strip()=='## Содержание')
    b=next(i for i,line in enumerate(md) if line.startswith('# Глава 1.'))
    md=md[:a]+md[b:]
except StopIteration: pass
body=[]
# title page
body += [p('',first=False),p('',first=False),p('',first=False),p('',first=False),p('ОТЧЕТ ПО КУРСОВОМУ ПРОЕКТУ',align='center',bold=True,first=False),
         p('«Нечеткие величины на основе равномерного закона U(a,b): генерация, операции над α-срезами и кластеризация»',align='center',bold=True,first=False),
         p('',first=False),p('',first=False),
         p('Образовательная организация: Московский авиационный институт',first=False),
         p('Институт: Институт компьютерных наук (Институт №8)',first=False),
         p('Дисциплина: «Неклассические логики»',first=False),
         p('Студент: ________________________________    Группа: __________',first=False),
         p('Руководитель: Д.В. Кубраков, преподаватель ОЦ-8',first=False),
         p('',first=False),p('',first=False),p('',first=False),
         p('Москва, 2026',align='center',first=False),
         E('w:p',children=[E('w:r',children=[empty('w:br',attrs={'w:type':'page'})])])]

i=0
in_formula=False
formula=[]
while i<len(md):
    line=md[i]
    if line.startswith('---'):
        i+=1; continue
    if line.strip()=='':
        i+=1; continue
    if line.strip()=='\\[':
        formula=[]; in_formula=True; i+=1; continue
    if in_formula:
        if line.strip()=='\\]':
            body.append(formula_paragraph(' '.join(formula))); in_formula=False
        else: formula.append(line.strip())
        i+=1; continue
    m=re.match(r'^(#{1,3})\s+(.*)$',line)
    if m:
        body.append(heading(m.group(2),len(m.group(1)))); i+=1; continue
    if line.startswith('|') and i+1<len(md) and md[i+1].startswith('|'):
        rows=[]
        while i<len(md) and md[i].startswith('|'):
            vals=[x.strip() for x in md[i].strip().strip('|').split('|')]
            if not all(re.fullmatch(r'[-: ]+',x or '-') for x in vals): rows.append(vals)
            i+=1
        body.append(table(rows)); continue
    m=re.match(r'^[-*]\s+(.*)$',line)
    if m: body.append(bullet(m.group(1))); i+=1; continue
    m=re.match(r'^\d+\.\s+(.*)$',line)
    if m: body.append(number_item(m.group(1))); i+=1; continue
    if line.startswith('[Вставить'):
        body.append(p(line,align='center',italic=True,first=False)); i+=1; continue
    if line.startswith('**Ключевые слова:**'):
        body.append(p(line,bold=False,first=True)); i+=1; continue
    body.append(p(line)); i+=1

# styles and document XML
styles = f'''<w:styles xmlns:w="{W}">
<w:docDefaults><w:rPrDefault><w:rPr>{rpr(size=28)}</w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:line="360" w:after="0"/><w:ind w:firstLine="709"/></w:pPr></w:pPrDefault></w:docDefaults>
<w:style w:type="paragraph" w:styleId="Normal"><w:name w:val="Normal"/><w:rPr>{rpr(size=28)}</w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="Heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:outlineLvl w:val="0"/><w:pPr><w:keepNext/><w:spacing w:before="240" w:after="120"/><w:ind w:firstLine="0"/></w:pPr><w:rPr>{rpr(bold=True,size=32)}</w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="Heading 2"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:outlineLvl w:val="1"/><w:pPr><w:keepNext/><w:spacing w:before="180" w:after="80"/><w:ind w:firstLine="0"/></w:pPr><w:rPr>{rpr(bold=True,size=30)}</w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading3"><w:name w:val="Heading 3"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:outlineLvl w:val="2"/><w:pPr><w:keepNext/><w:spacing w:before="120" w:after="60"/><w:ind w:firstLine="0"/></w:pPr><w:rPr>{rpr(bold=True,size=28)}</w:rPr></w:style>
</w:styles>'''
sect=E('w:sectPr',children=[E('w:pgSz',attrs={'w:w':'11906','w:h':'16838'}),E('w:pgMar',attrs={'w:top':'1134','w:right':'567','w:bottom':'1134','w:left':'1701','w:header':'720','w:footer':'720','w:gutter':'0'}),E('w:footerReference',attrs={'w:type':'default','r:id':'rIdFooter'}),E('w:pgNumType',attrs={'w:fmt':'decimal'})])
# Add TOC field after title page by inserting before first heading
for idx,b in enumerate(body):
    if 'w:style w:val="Heading1"' in b:
        body.insert(idx, p('СОДЕРЖАНИЕ',align='center',bold=True,first=False))
        toc_lines = ['Глава 1. Введение и постановка задачи .................................. 3', 'Глава 2. Теоретические и алгоритмические основы .................... 6', 'Глава 3. Архитектура и программная реализация ........................ 11', 'Глава 4. Экспериментальное исследование и результаты ............. 15', 'Заключение ........................................................................ 19', 'Список использованных источников ........................................ 20']
        for j,line in enumerate(toc_lines): body.insert(idx+1+j,p(line,first=False))
        body.insert(idx+1+len(toc_lines),E('w:p',children=[E('w:r',children=[empty('w:br',attrs={'w:type':'page'})])]))
        break

doc=f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<w:document xmlns:w="{W}" xmlns:r="{R}" xmlns:m="{M}"><w:body>{''.join(body)}{sect}</w:body></w:document>'''
footer=f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:ftr xmlns:w="{W}"><w:p><w:pPr><w:jc w:val="center"/></w:pPr><w:fldSimple w:instr="PAGE"><w:r><w:t>1</w:t></w:r></w:fldSimple></w:p></w:ftr>'''
content_types=f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="{CT}"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/><Override PartName="/word/footer1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml"/></Types>'''
rels=f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="{PKG}"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>'''
docrels=f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="{PKG}"><Relationship Id="rIdStyles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/><Relationship Id="rIdFooter" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer" Target="footer1.xml"/></Relationships>'''
with zipfile.ZipFile(OUT,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml',content_types)
    z.writestr('_rels/.rels',rels)
    z.writestr('word/document.xml',doc)
    z.writestr('word/styles.xml',styles)
    z.writestr('word/footer1.xml',footer)
    z.writestr('word/_rels/document.xml.rels',docrels)
print(OUT)
